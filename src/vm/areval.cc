// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "runtime.h"

#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/nil.h>
#include <object/datatype/list.h>
#include <object/datatype/tuple.h>
#include <object/datatype/bounds.h>
#include <object/datatype/map.h>
#include <object/datatype/function.h>
#include <object/datatype/trait.h>
#include <object/datatype/struct.h>
#include <object/datatype/instance.h>
#include <object/arobject.h>

#include <lang/opcodes.h>
#include "areval.h"

using namespace argon::vm;
using namespace argon::object;

ArObject *Binary(ArRoutine *routine, ArObject *l, ArObject *r, int offset) {
#define GET_BINARY_OP(struct, offset) *((BinaryOp *) (((unsigned char *) struct) + offset))
    BinaryOp lop = GET_BINARY_OP(l->type->ops, offset);
    BinaryOp rop = GET_BINARY_OP(r->type->ops, offset);

    ArObject *result = nullptr;

    if (lop != nullptr)
        result = lop(l, r);
    if (rop != nullptr && result == nullptr && !RoutineIsPanicking(routine))
        result = rop(l, r);

    return result;
#undef GET_BINARY_OP
}

ArObject *MkBounds(Frame *frame, unsigned short args) {
    arsize step = 1;
    arsize stop = 0;
    arsize start;

    ArObject *obj = *(frame->eval_stack - 1);

    if (args == 3) {
        if (!AsIndex(obj))
            return ErrorFormat(&error_type_error, "step parameter must be integer not '%s'", obj->type->name);

        step = obj->type->number_actions->as_index(obj);
        Release(*(--frame->eval_stack));
        obj = *(frame->eval_stack - 1);
    }

    if (args >= 2) {
        if (!AsIndex(obj))
            return ErrorFormat(&error_type_error, "stop parameter must be integer not '%s'", obj->type->name);

        stop = obj->type->number_actions->as_index(obj);
        Release(*(--frame->eval_stack));
        obj = *(frame->eval_stack - 1);
    }

    if (!AsIndex(obj))
        return ErrorFormat(&error_type_error, "start parameter must be integer not '%s'", obj->type->name);

    start = obj->type->number_actions->as_index(obj);

    return BoundsNew(start, stop, step);
}

ArObject *MkCurrying(Function *fn_old, ArObject **args, size_t count) {
    List *currying = ListNew(count);

    if (currying == nullptr)
        return nullptr;

    for (size_t i = 0; i < count; i++) {
        if (!ListAppend(currying, args[i])) {
            Release(currying);
            return nullptr;
        }
    }

    Function *fn_new = FunctionNew(fn_old, currying);
    Release(currying);

    return fn_new;
}

bool GetMRO(ArRoutine *routine, List **mro, Trait **impls, size_t count) {
    (*mro) = nullptr;

    if (count > 0) {
        List *bases = BuildBasesList((Trait **) impls, count);
        if (bases == nullptr)
            return false;

        (*mro) = ComputeMRO(bases);
        Release(bases);

        if ((*mro) == nullptr)
            return false; // TODO: user error

        if ((*mro)->len == 0) {
            Release((*mro));
            return false; // TODO: impls error
        }
    }

    return true;
}

ArObject *MkConstruct(ArRoutine *routine, Code *code, String *name, Trait **impls, size_t count, bool is_trait) {
    ArObject *ret;
    Namespace *ns;
    Frame *frame;
    List *mro;

    if ((ns = NamespaceNew()) == nullptr)
        return nullptr;

    if ((frame = FrameNew(code, routine->frame->globals, ns)) == nullptr) {
        Release(ns);
        return nullptr;
    }

    Eval(routine, frame);
    FrameDel(frame);

    if (!GetMRO(routine, &mro, impls, count)) {
        Release(ns);
        return nullptr;
    }

    ret = is_trait ?
          (ArObject *) TraitNew(name, ns, mro) :
          (ArObject *) StructNew(name, ns, mro);

    Release(ns);
    Release(mro);

    return ret;
}

ArObject *InstantiateStruct(ArRoutine *routine, Struct *base, ArObject **values, size_t count, bool key_pair) {
    Instance *instance;
    Namespace *instance_ns;

    if ((instance_ns = NamespaceNew()) == nullptr)
        return nullptr;

    if (!key_pair) {
        size_t index = 0;
        for (NsEntry *cur = base->names->iter_begin; cur != nullptr; cur = cur->iter_next) {
            if (cur->info.IsMember() && !cur->info.IsConstant()) {
                ArObject *value;

                if (index < count) {
                    value = values[index++];
                    IncRef(value);
                } else
                    value = NamespaceGetValue(base->names, cur->key, nullptr);

                bool ok = NamespaceNewSymbol(instance_ns, cur->info, cur->key, value);
                Release(value);

                if (!ok) {
                    Release(instance_ns);
                    return nullptr;
                }
            }
        }
    } else {
        // Load default values
        for (NsEntry *cur = base->names->iter_begin; cur != nullptr; cur = cur->iter_next) {
            if (cur->info.IsMember())
                if (!NamespaceNewSymbol(instance_ns, cur->info, cur->key, cur->obj)) {
                    Release(instance_ns);
                    return nullptr;
                }
        }

        for (size_t i = 0; i < count; i += 2) {
            if (!NamespaceSetValue(instance_ns, values[i], values[i + 1])) {
                Release(instance_ns);
                ErrorFormat(&error_undeclared_variable, "'%s' undeclared struct property",
                            ((String *) values[i])->buffer);
                return nullptr;
            }
        }
    }

    instance = InstanceNew(base, instance_ns);
    Release(instance_ns);

    return instance;
}

ArObject *Subscript(ArObject *obj, ArObject *idx, ArObject *set) {
    ArObject *ret = nullptr;

    if (IsMap(obj)) {
        if (set == nullptr) {
            if ((ret = obj->type->map_actions->get_item(obj, idx)) == nullptr)
                return nullptr;
        } else {
            if (!obj->type->map_actions->set_item(obj, idx, set))
                return False;
        }
    } else if (IsSequence(obj)) {
        if (AsIndex(idx)) {
            if (set == nullptr) {
                ret = obj->type->sequence_actions->get_item(obj, idx->type->number_actions->as_index(idx));
                if (ret == nullptr)
                    return nullptr;
            } else {
                if (!obj->type->sequence_actions->set_item(obj, set, idx->type->number_actions->as_index(idx)))
                    return False;
            }
        } else if (idx->type == &type_bounds_) {
            if (set == nullptr) {
                ret = obj->type->sequence_actions->get_slice(obj, idx);
            } else {
                if (!obj->type->sequence_actions->set_slice(obj, idx, set))
                    return False;
            }
        } else {
            ErrorFormat(&error_type_error, "sequence index must be integer or bounds not '%s'",
                        idx->type->name);
            return nullptr;
        }
    } else {
        ErrorFormat(&error_type_error, "'%s' not subscriptable", obj->type->name);
        return nullptr;
    }

    return ret;
}

ArObject *LoadStoreAttribute(ArObject *obj, ArObject *key, ArObject *set) {
    ArObject *ret = nullptr;


    if (obj->type->obj_actions == nullptr || ((set != nullptr && obj->type->obj_actions->set_attr == nullptr) ||
                                              obj->type->obj_actions->get_attr == nullptr)) {
        ErrorFormat(&error_attribute_error, "'%s' object is unable to use the attribute(.) operator", obj->type->name);
        return set != nullptr ? False : nullptr;
    }

    if (set == nullptr)
        ret = obj->type->obj_actions->get_attr(obj, key);
    else {
        if (!obj->type->obj_actions->set_attr(obj, key, set))
            return False;
    }

    return ret;
}

ArObject *LoadStoreScope(ArObject *obj, ArObject *key, ArObject *set) {
    ArObject *ret = nullptr;


    if (obj->type->obj_actions == nullptr || ((set != nullptr && obj->type->obj_actions->set_static_attr == nullptr) ||
                                              obj->type->obj_actions->get_static_attr == nullptr)) {
        ErrorFormat(&error_attribute_error, "'%s' object is unable to use scope(::) operator", obj->type->name);
        return set != nullptr ? False : nullptr;
    }

    if (set == nullptr)
        ret = obj->type->obj_actions->get_static_attr(obj, key);
    else {
        if (!obj->type->obj_actions->set_static_attr(obj, key, set))
            return False;
    }

    return ret;
}

ArObject *NativeCall(ArRoutine *routine, Function *function, ArObject **args, size_t count) {
    List *arguments = nullptr;
    ArObject **raw = nullptr;

    if (function->arity > 0) {
        raw = args;

        /*
        if (count < function->arity) {
            if ((arguments = ListNew(function->arity)) == nullptr)
                return nullptr;

            if (function->currying != nullptr) {
                if (!ListConcat(arguments, function->currying)) {
                    Release(arguments);
                    return nullptr;
                }
            }

            for (size_t i = 0; i < count; i++)
                ListAppend(arguments, args[i]);

            if (function->variadic && arguments->len + 1 == function->arity)
                ListAppend(arguments, NilVal);

            raw = arguments->objects;
        }
         */
    }

    auto res = function->native_fn(function, raw);
    Release(arguments);

    return res;
}

ArObject *RestElementToList(ArObject **args, size_t count) {
    List *rest = ListNew(count);

    if (rest != nullptr) {
        for (size_t i = 0; i < count; i++) {
            if (!ListAppend(rest, args[i])) {
                Release(rest);
                return nullptr;
            }
        }
    }

    return rest;
}

void FillFrameForCall(Frame *frame, Function *callable, ArObject **args, size_t count) {
    size_t local_idx = 0;

    // If method, first parameter must be an instance
    if (callable->instance != nullptr) {
        frame->instance = callable->instance;
        frame->locals[local_idx++] = callable->instance;
        IncRef(callable->instance);
    }

    // Push currying args
    if (callable->currying != nullptr) {
        for (size_t i = 0; i < callable->currying->len; i++)
            frame->locals[local_idx++] = ListGetItem(callable->currying, i);
    }

    // Fill with stack args
    for (size_t i = 0; i < count; i++)
        frame->locals[local_idx++] = args[i];

    // If last parameter in variadic function is empty, fill it with NilVal
    if (callable->variadic && local_idx + 1 == callable->arity)
        frame->locals[local_idx++] = ReturnNil();

    assert(local_idx == callable->arity);

    // Fill with enclosed args
    if (callable->enclosed != nullptr) {
        for (size_t i = 0; i < callable->enclosed->len; i++)
            frame->enclosed[i] = ListGetItem(callable->enclosed, i);
    }
}

ArObject *argon::vm::Eval(ArRoutine *routine, Frame *frame) {
    ArObject *ret;

    frame->back = routine->frame;
    routine->frame = frame;
    ret = Eval(routine);
    routine->frame = frame->back;

    return ret;
}

ArObject *argon::vm::Eval(ArRoutine *routine) {
#ifndef COMPUTED_GOTO
#define TARGET_OP(op)   \
    case argon::lang::OpCodes::op:

#define DISPATCH()                                      \
    cu_frame->instr_ptr+=sizeof(argon::lang::Instr8);   \
    continue

#define DISPATCH2()                                     \
    cu_frame->instr_ptr+=sizeof(argon::lang::Instr16);  \
    continue

#define DISPATCH4()                                     \
    cu_frame->instr_ptr+=sizeof(argon::lang::Instr32);  \
    continue

#else
#define TARGET_OP(op)   \
    case OpCodes::op:   \
    LBL_##op:
#endif

#define JUMPTO(offset)                                              \
    cu_frame->instr_ptr = (unsigned char *)cu_code->instr + offset; \
    continue

// ARGUMENT
#define ARG16 argon::lang::I16Arg(cu_frame->instr_ptr)
#define ARG32 argon::lang::I32Arg(cu_frame->instr_ptr)

// STACK MANIPULATION MACRO
#define PUSH(obj)                   \
    *cu_frame->eval_stack = obj;    \
    cu_frame->eval_stack++
#define POP()       Release(*(--cu_frame->eval_stack))
#define STACK_REWIND(offset) for(size_t i = offset; i>0; POP(), i--)
#define TOP()       (*(cu_frame->eval_stack-1))
#define TOP_REPLACE(obj)                \
    Release(*(cu_frame->eval_stack-1)); \
    *(cu_frame->eval_stack-1)=obj
#define PEEK1()     (*(cu_frame->eval_stack-2))
#define PEEK2()     (*(cu_frame->eval_stack-3))

#define BINARY_OP(routine, op, opchar)                                                          \
    if ((ret = Binary(routine, PEEK1(), TOP(), offsetof(OpSlots, op))) == nullptr) {            \
        if (!RoutineIsPanicking(routine)) {                                                      \
            ErrorFormat(&error_type_error, "unsupported operand type '%s' for: '%s' and '%s'",  \
            #opchar, PEEK1()->type->name, TOP()->type->name);                                   \
        }                                                                                       \
        goto error;                                                                             \
    }                                                                                           \
    POP();                                                                                      \
    TOP_REPLACE(ret);                                                                           \
    DISPATCH()

#define UNARY_OP(op)                                \
    ret = TOP();                                    \
    if((ret = ret->type->ops->op(ret)) == nullptr)  \
        goto error;                                 \
    TOP_REPLACE(ret);                               \
    DISPATCH()

    // FUNCTION START
    ArObject *last_popped = ReturnNil();
    Frame *first_frame = routine->frame;

    begin:
    Frame *cu_frame = routine->frame;
    Code *cu_code = cu_frame->code;

    ArObject *ret = nullptr;

    while (cu_frame->instr_ptr < (cu_code->instr + cu_code->instr_sz)) {
        switch (*(argon::lang::OpCodes *) cu_frame->instr_ptr) {
            TARGET_OP(ADD) {
                BINARY_OP(routine, add, +);
            }
            TARGET_OP(CALL) {
                auto local_args = ARG16;
                auto func = (Function *) *(cu_frame->eval_stack - local_args - 1);
                unsigned short total_args = local_args;
                unsigned short arity = func->arity;

                if (func->type != &type_function_) {
                    ErrorFormat(&error_type_error, "'%s' object is not callable", func->type->name);
                    goto error;
                }

                if (func->instance != nullptr) total_args++;

                if (func->currying != nullptr) total_args += func->currying->len;

                if (func->variadic) arity--;

                if (total_args < arity) {
                    if (total_args == 0) {
                        ErrorFormat(&error_type_error, "%s() takes %d argument, but 0 were given",
                                    ((String *) func->name)->buffer, func->arity);
                        goto error;
                    }

                    if ((ret = MkCurrying(func, cu_frame->eval_stack - local_args, local_args)) == nullptr)
                        goto error;

                    STACK_REWIND(local_args);
                    TOP_REPLACE(ret);
                    DISPATCH2();
                } else if (total_args > arity) {
                    unsigned short exceeded = total_args - arity;

                    if (!func->variadic) {
                        ErrorFormat(&error_type_error, "%s() takes %d argument, but %d were given",
                                    ((String *) func->name)->buffer, func->arity, total_args);
                        goto error;
                    }

                    if ((ret = RestElementToList(cu_frame->eval_stack - exceeded, exceeded)) == nullptr)
                        goto error;

                    local_args -= exceeded - 1;
                    STACK_REWIND(exceeded - 1);
                    TOP_REPLACE(ret);
                }

                if (func->native) {
                    if ((ret = NativeCall(routine, func, cu_frame->eval_stack - local_args, local_args)) == nullptr)
                        goto error;

                    STACK_REWIND(local_args);
                    TOP_REPLACE(ret);
                    DISPATCH2();
                }

                // Argon Code
                auto func_frame = FrameNew(func->code, func->gns, nullptr); // TODO: check proxy_globals
                if (func_frame == nullptr)
                    goto error;

                FillFrameForCall(func_frame, func, cu_frame->eval_stack - local_args, local_args);
                cu_frame->eval_stack -= local_args;

                POP(); // pop function!

                // Invoke:
                cu_frame->instr_ptr += sizeof(argon::lang::Instr16);
                func_frame->back = cu_frame;
                routine->frame = func_frame;
                goto begin;
            }
            TARGET_OP(CMP) {
                auto mode = (CompareMode) ARG16;
                auto left = PEEK1();

                if (mode == CompareMode::EQ)
                    ret = BoolToArBool(left->type->equal(left, TOP()));
                else if (mode == CompareMode::NE)
                    ret = BoolToArBool(!left->type->equal(left, TOP()));
                else {
                    if (left->type->compare == nullptr) {
                        ErrorFormat(&error_not_implemented,
                                    "'%s' does not support any comparison operator",
                                    left->type->name);
                        goto error;
                    }
                    ret = left->type->compare(left, TOP(), mode);
                }
                POP();
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(DEC) {
                UNARY_OP(dec);
            }
            TARGET_OP(DFR) {
                auto local_args = ARG16;
                auto func = (Function *) *(cu_frame->eval_stack - local_args - 1);
                unsigned short total_args = local_args;
                unsigned short arity = func->arity;

                if (func->type != &type_function_) {
                    ErrorFormat(&error_type_error, "'%s' object is not callable", func->type->name);
                    goto error;
                }

                if (func->variadic) arity--;

                if (func->instance != nullptr) total_args++;

                if (arity != total_args) {
                    ErrorFormat(&error_type_error, "%s() takes %d argument, but %d were given",
                                ((String *) func->name)->buffer, func->arity, total_args);
                    goto error;
                }

                if ((ret = MkCurrying(func, cu_frame->eval_stack - local_args, local_args)) == nullptr)
                    goto error;

                RoutineNewDefer(routine, ret);
                Release(ret);

                STACK_REWIND(local_args + 1); // +1 func it self
                DISPATCH2();
                // MISS YOU E.V.
            }
            TARGET_OP(DIV) {
                BINARY_OP(routine, div, /);
            }
            TARGET_OP(DUP) {
                // TODO: CHECK OutOfBound on stack
                auto back = ARG16;
                ArObject **cursor = cu_frame->eval_stack - back;

                while (back-- > 0) {
                    IncRef(*cursor);
                    PUSH(*(cursor++));
                }

                DISPATCH2();
            }
            TARGET_OP(IDIV) {
                BINARY_OP(routine, idiv, '//');
            }
            TARGET_OP(IMPFRM) {
                auto attribute = (String *) TupleGetItem(cu_code->statics, ARG32);

                if ((ret = LoadStoreScope(TOP(), attribute, nullptr)) == nullptr) {
                    Release(attribute);
                    goto error;
                }

                Release(attribute);
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(IMPMOD) {
                auto path = (String *) TupleGetItem(cu_code->statics, ARG32);

                if ((ret = ImportModule(routine->context->import, path, nullptr)) == nullptr) {
                    Release(path);
                    goto error;
                }

                Release(path);
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(INC) {
                UNARY_OP(inc);
            }
            TARGET_OP(INIT) {
                auto args = ARG16;
                bool key_pair = argon::lang::I32ExtractFlag(cu_frame->instr_ptr);
                auto bstruct = (Struct *) *(cu_frame->eval_stack - args - 1);

                if (bstruct->type != &type_struct_) {
                    ErrorFormat(&error_type_error, "expected struct, found '%s'", bstruct->type->name);
                    goto error;
                }

                if (!key_pair && args > bstruct->properties_count) {
                    ErrorFormat(&error_type_error, "'%s' takes %d positional arguments but %d were given",
                                bstruct->name->buffer, bstruct->properties_count, args);
                    goto error;
                }

                if ((ret = InstantiateStruct(routine, bstruct, cu_frame->eval_stack - args, args, key_pair)) == nullptr)
                    goto error;

                STACK_REWIND(args);
                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(INV) {
                UNARY_OP(invert);
            }
            TARGET_OP(IPADD) {
                BINARY_OP(routine, inp_add, +=);
            }
            TARGET_OP(IPDIV) {
                BINARY_OP(routine, inp_div, /=);
            }
            TARGET_OP(IPMUL) {
                BINARY_OP(routine, inp_add, *=);
            }
            TARGET_OP(IPSUB) {
                BINARY_OP(routine, inp_add, -=);
            }
            TARGET_OP(JF) {
                // JUMP FALSE
                if (!IsTrue(TOP())) {
                    POP();
                    JUMPTO(ARG32);
                }
                POP();
                DISPATCH4();
            }
            TARGET_OP(JFOP) {
                // JUMP FALSE OR POP
                if (IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(ARG32);
            }
            TARGET_OP(JMP) {
                JUMPTO(ARG32);
            }
            TARGET_OP(JNIL) {
                // JUMP IF NIL
                if (TOP() == NilVal) {
                    JUMPTO(ARG32);
                }
                DISPATCH4();
            }
            TARGET_OP(JT) {
                // JUMP IF TRUE
                if (IsTrue(TOP())) {
                    POP();
                    JUMPTO(ARG32);
                }
                POP();
                DISPATCH4();
            }
            TARGET_OP(JTOP) {
                // JUMP TRUE OR POP
                if (!IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(ARG32);
            }
            TARGET_OP(LAND) {
                BINARY_OP(routine, l_and, &);
            }
            TARGET_OP(LDATTR) {
                // TODO: CHECK OutOfBound
                ArObject *key = TupleGetItem(cu_code->statics, ARG32);

                if ((ret = LoadStoreAttribute(TOP(), key, nullptr)) == nullptr) {
                    Release(key);
                    goto error;
                }
                Release(key);

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(LDENC) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                IncRef(cu_frame->enclosed[idx]);
                PUSH(cu_frame->enclosed[idx]);
                DISPATCH2();
            }
            TARGET_OP(LDGBL) {
                // TODO: CHECK OutOfBound
                auto *key = TupleGetItem(cu_code->names, ARG16);

                ret = NamespaceGetValue(cu_frame->globals, key, nullptr);

                if (ret == nullptr && cu_frame->proxy_globals != nullptr)
                    ret = NamespaceGetValue(cu_frame->proxy_globals, key, nullptr);

                // Check builtins
                if (ret == nullptr)
                    ret = NamespaceGetValue(GetContext()->bltins->module_ns, key, nullptr);

                Release(key);

                if (ret == nullptr) {
                    ErrorFormat(&error_undeclared_variable, "'%s' undeclared global variable",
                                ((String *) key)->buffer);
                    goto error;
                }

                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(LDLC) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                IncRef(cu_frame->locals[idx]);
                PUSH(cu_frame->locals[idx]);
                DISPATCH2();
            }
            TARGET_OP(LDSCOPE) {
                // TODO: CHECK OutOfBound
                ArObject *key = TupleGetItem(cu_code->statics, ARG32);

                if ((ret = LoadStoreScope(TOP(), key, nullptr)) == nullptr) {
                    Release(key);
                    goto error;
                }
                Release(key);

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(LOR) {
                BINARY_OP(routine, l_or, |);
            }
            TARGET_OP(LSTATIC) {
                // TODO: CHECK OutOfBound
                ret = TupleGetItem(cu_code->statics, ARG32);
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(LXOR) {
                BINARY_OP(routine, l_xor, ^);
            }
            TARGET_OP(MK_BOUNDS) {
                if ((ret = MkBounds(cu_frame, ARG16)) == nullptr)
                    goto error;

                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_FUNC) {
                auto flags = (argon::lang::MkFuncFlags) argon::lang::I32ExtractFlag(cu_frame->instr_ptr);
                auto name = (String *) PEEK1();
                List *enclosed = nullptr;

                ret = TOP();

                if (ENUMBITMASK_ISTRUE(flags, argon::lang::MkFuncFlags::CLOSURE)) {
                    enclosed = (List *) TOP();
                    ret = PEEK1();
                    name = (String *) PEEK2();
                }

                ret = FunctionNew(cu_frame->globals, name, (Code *) ret, ARG16,
                                  ENUMBITMASK_ISTRUE(flags, argon::lang::MkFuncFlags::VARIADIC),
                                  enclosed);

                if (ret == nullptr)
                    goto error;

                if (ENUMBITMASK_ISTRUE(flags, argon::lang::MkFuncFlags::CLOSURE))
                    POP();

                POP();
                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_LIST) {
                auto args = ARG32;

                if ((ret = ListNew(args)) == nullptr)
                    goto error;

                // Fill list
                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    if (!ListAppend((List *) ret, *cursor))
                        goto error;
                    Release(*cursor);
                }

                cu_frame->eval_stack -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_MAP) {
                auto args = ARG32 * 2;

                if ((ret = MapNew()) == nullptr)
                    goto error;

                // Fill map
                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor += 2) {
                    if (!MapInsert((Map *) ret, *cursor, *(cursor + 1)))
                        goto error;
                    Release(*cursor);
                    Release(*(cursor + 1));
                }

                cu_frame->eval_stack -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_SET) {

            }
            TARGET_OP(MK_STRUCT) {
                auto args = ARG16;

                ret = MkConstruct(routine, (Code *) *(cu_frame->eval_stack - args - 2),
                                  (String *) *(cu_frame->eval_stack - args - 1),
                                  (Trait **) (cu_frame->eval_stack - args), args, false);
                if (ret == nullptr)
                    goto error;

                STACK_REWIND(args + 1); // args + 1(name)
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_TRAIT) {
                auto args = ARG16;

                ret = MkConstruct(routine, (Code *) *(cu_frame->eval_stack - args - 2),
                                  (String *) *(cu_frame->eval_stack - args - 1),
                                  (Trait **) (cu_frame->eval_stack - args), args, true);
                if (ret == nullptr)
                    goto error;

                STACK_REWIND(args + 1); // args + 1(name)
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_TUPLE) {
                auto args = ARG32;

                if ((ret = TupleNew(args)) == nullptr)
                    goto error;

                // Fill tuple
                unsigned int idx = 0;
                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    if (!TupleInsertAt((Tuple *) ret, idx++, *cursor))
                        goto error;
                    Release(*cursor);
                }

                cu_frame->eval_stack -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MOD) {
                BINARY_OP(routine, module, %);
            }
            TARGET_OP(MUL) {
                BINARY_OP(routine, mul, *);
            }
            TARGET_OP(NEG) {
                UNARY_OP(neg);
            }
            TARGET_OP(NGV) {
                // TODO: CHECK OutOfBound
                auto map = cu_frame->proxy_globals != nullptr ?
                           cu_frame->proxy_globals : cu_frame->globals;

                ret = TupleGetItem(cu_code->names, ARG16);

                if (!NamespaceNewSymbol(map, PropertyInfo((PropertyType) (ARG32 >> (unsigned char) 16)), ret, TOP()))
                    goto error;

                Release(ret);
                POP();
                DISPATCH4();
            }
            TARGET_OP(NLV) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                IncRef(TOP());
                cu_frame->locals[idx] = TOP();
                POP();
                DISPATCH2();
            }
            TARGET_OP(NOT) {
                ret = True;
                if (IsTrue(TOP()))
                    ret = False;
                IncRef(ret);
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(POP) {
                Release(last_popped);
                last_popped = TOP();
                cu_frame->eval_stack--; // DO NOT RELEASE, it may be returned as a return value!
                DISPATCH();
            }
            TARGET_OP(POS) {
                UNARY_OP(pos);
            }
            TARGET_OP(PB_HEAD) {
                auto len = ARG16;

                ret = TOP(); // Save TOP

                for (size_t i = 0; i < len - 1; i++)
                    *(cu_frame->eval_stack - i - 1) = *(cu_frame->eval_stack - i - 2);

                *(cu_frame->eval_stack - len) = ret;
                DISPATCH2();
            }
            TARGET_OP(RET) {
                if (routine->defer != nullptr && routine->defer->frame == cu_frame->back) {
                    if (cu_frame->stack_extra_base != cu_frame->eval_stack) {
                        POP();
                    }
                    goto end_while;
                }

                if (cu_frame->stack_extra_base == cu_frame->eval_stack)
                    *cu_frame->back->eval_stack = ReturnNil();
                else {
                    *cu_frame->back->eval_stack = TOP();
                    cu_frame->eval_stack--;
                }

                cu_frame->back->eval_stack++;
                goto end_while;
            }
            TARGET_OP(SHL) {
                BINARY_OP(routine, shl, >>);
            }
            TARGET_OP(SHR) {
                BINARY_OP(routine, shr, <<);
            }
                /*TARGET_OP(SPWN){}*/
            TARGET_OP(STATTR) {
                // TODO: CHECK OutOfBound
                ArObject *key = TupleGetItem(cu_code->statics, ARG32);

                ret = LoadStoreAttribute(TOP(), key, PEEK1());
                if (ret == False) {
                    Release(key);
                    goto error;
                }

                Release(key);
                POP(); // Value
                POP(); // Instance
                DISPATCH4();
            }
            TARGET_OP(STENC) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                Release(cu_frame->enclosed[idx]);
                cu_frame->enclosed[idx] = TOP();
                cu_frame->eval_stack--;
                DISPATCH2();
            }
            TARGET_OP(STGBL) {
                // TODO: CHECK OutOfBound
                auto map = cu_frame->proxy_globals != nullptr ?
                           cu_frame->proxy_globals : cu_frame->globals;
                PropertyInfo pinfo{};

                ret = TupleGetItem(cu_code->names, ARG16);

                if (!NamespaceContains(map, ret, &pinfo)) {
                    ErrorFormat(&error_undeclared_variable, "'%s' undeclared global variable in assignment",
                                ((String *) ret)->buffer);
                    goto error;
                }

                if (pinfo.IsConstant()) {
                    ErrorFormat(&error_unassignable_variable, "unable to assign value to '%s' because is constant",
                                ((String *) ret)->buffer);
                    goto error;
                }

                NamespaceSetValue(map, ret, TOP());
                Release(ret);
                POP();
                DISPATCH4();
            }
            TARGET_OP(STLC) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                Release(cu_frame->locals[idx]);
                cu_frame->locals[idx] = TOP();
                cu_frame->eval_stack--;
                DISPATCH2();
            }
            TARGET_OP(STSCOPE) {
                ArObject *key = TupleGetItem(cu_code->statics, ARG32);

                ret = LoadStoreScope(TOP(), key, PEEK1());
                if (ret == False) {
                    Release(key);
                    goto error;
                }

                Release(key);
                POP(); // Value
                POP(); // Instance
                DISPATCH4();
            }
            TARGET_OP(STSUBSCR) {
                if ((ret = Subscript(PEEK1(), TOP(), PEEK2())) == False)
                    goto error;
                STACK_REWIND(3);
                DISPATCH();
            }
            TARGET_OP(SUB) {
                BINARY_OP(routine, sub, -);
            }
            TARGET_OP(SUBSCR) {
                if ((ret = Subscript(PEEK1(), TOP(), nullptr)) == nullptr)
                    goto error;
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(TEST) {
                ret = PEEK1();
                if (ret->type->equal(ret, TOP())) {
                    POP();
                    TOP_REPLACE(BoolToArBool(true));
                    DISPATCH();
                }
                TOP_REPLACE(BoolToArBool(false));
                DISPATCH();
            }
            TARGET_OP(UNPACK) {
                auto len = ARG32;
                ret = TOP();

                if (!IsSequence(ret)) {
                    ErrorFormat(&error_type_error, "unpacking expression was expecting a sequence not a '%s'",
                                ret->type->name);
                    goto error;
                }

                size_t s_len = ret->type->sequence_actions->length(ret);
                if (s_len != len) {
                    ErrorFormat(&error_value_error, "incompatible number of value to unpack (expected '%d' got '%d')",
                                len, s_len);
                    goto error;
                }

                cu_frame->eval_stack--;
                while (len-- > 0) {
                    PUSH(ret->type->sequence_actions->get_item(ret, len));
                }

                Release(ret);
                DISPATCH4();
            }
            default:
                ErrorFormat(&error_runtime_error, "unknown opcode: 0x%X", (unsigned char) (*cu_frame->instr_ptr));
        }
        error:
        STACK_REWIND(cu_frame->eval_stack - cu_frame->stack_extra_base);
        break;
    }

    end_while:

    assert(cu_frame->stack_extra_base == cu_frame->eval_stack);

    // DEFERRED
    while (routine->defer != nullptr && routine->defer->frame == cu_frame) {
        auto dfr_fn = (Function *) routine->defer->function;

        routine->defer->panic = routine->panic;
        routine->cu_defer = routine->defer;

        if (!dfr_fn->native) {
            auto dfr_frame = FrameNew(dfr_fn->code, dfr_fn->gns, nullptr); // TODO: check proxy_globals
            FillFrameForCall(dfr_frame, dfr_fn, nullptr, 0);
            Release(Eval(routine, dfr_frame));
            FrameDel(dfr_frame);
            RoutineDelDefer(routine);
        } else
            Release(NativeCall(routine, dfr_fn, nullptr, 0));
    }

    if (routine->frame->back != nullptr && routine->frame != first_frame) {
        routine->frame = cu_frame->back;
        FrameDel(cu_frame);

        if (RoutineIsPanicking(routine)) {
            cu_frame = routine->frame;  // set cu_frame before jump to error
            goto error;
        }

        goto begin;
    }

    return last_popped;

#undef TARGET_OP
#undef DISPATCH
#undef DISPATCH2
#undef DISPATCH4
#undef JUMPTO

#undef ARG16
#undef ARG32

#undef PUSH
#undef POP
#undef STACK_REWIND
#undef TOP
#undef TOP_REPLACE
#undef PEEK1
#undef PEEK2

#undef BINARY_OP
#undef UNARY_OP
}