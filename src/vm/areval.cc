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
#include <object/datatype/set.h>
#include "areval.h"

using namespace argon::vm;
using namespace argon::object;

ArObject *Binary(ArRoutine *routine, ArObject *l, ArObject *r, int offset) {
#define GET_BINARY_OP(struct, offset) *((BinaryOp *) (((unsigned char *) (struct)) + (offset)))
    BinaryOp lop = nullptr;
    BinaryOp rop = nullptr;
    ArObject *result = nullptr;

    if (AR_GET_TYPE(l)->ops != nullptr)
        lop = GET_BINARY_OP(l->type->ops, offset);

    if (AR_GET_TYPE(r)->ops != nullptr)
        rop = GET_BINARY_OP(r->type->ops, offset);

    if (lop != nullptr)
        result = lop(l, r);
    if (rop != nullptr && result == nullptr && !RoutineIsPanicking(routine))
        result = rop(l, r);

    return result;
#undef GET_BINARY_OP
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

ArObject *ImportModule(ArRoutine *routine, String *name) {
    ImportSpec *spec;
    String *key;
    String *path;
    Module *module;

    if ((key = StringIntern("__spec")) == nullptr)
        return nullptr;

    spec = (ImportSpec *) NamespaceGetValue(routine->frame->globals, key, nullptr);

    path = nullptr;
    if (!IsNull(spec))
        path = spec->path;

    module = ImportModule(routine->context->import, name, path);

    Release(spec);
    Release(key);

    return module;
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
        for (auto *cur = (NsEntry *) base->names->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
            if (!cur->info.IsStatic() && !cur->info.IsConstant()) {
                ArObject *value;

                if (index < count) {
                    value = values[index++];
                    IncRef(value);
                } else
                    value = NamespaceGetValue(base->names, cur->key, nullptr);

                bool ok = NamespaceNewSymbol(instance_ns, cur->key, value, cur->info);
                Release(value);

                if (!ok) {
                    Release(instance_ns);
                    return nullptr;
                }
            }
        }
    } else {
        // Load default values
        for (auto *cur = (NsEntry *) base->names->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
            if (!cur->info.IsStatic())
                if (!NamespaceNewSymbol(instance_ns, cur->key, cur->value, cur->info)) {
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

    if (AsMap(obj)) {
        if (set == nullptr) {
            if ((ret = obj->type->map_actions->get_item(obj, idx)) == nullptr)
                return nullptr;
        } else {
            if (!obj->type->map_actions->set_item(obj, idx, set))
                return False;
        }
    } else if (AsSequence(obj)) {
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

ArObject *NativeCall(Function *function, ArObject **args, size_t count) {
    ArObject *instance = nullptr;
    List *arguments = nullptr;
    ArObject *ret;

    if (count > 0 && function->IsMethod()) {
        instance = *args;

        if (AR_GET_TYPE(instance) != function->base)
            return ErrorFormat(&error_type_error, "method '%s' for type '%s' doesn't apply to '%s' type",
                               function->name->buffer, function->base->name, AR_TYPE_NAME(instance));

        args++;
        count--;
    }

    if (function->arity > 0) {
        if (function->currying != nullptr) {
            if (args != nullptr && count > 0) {
                if ((arguments = ListNew(function->currying->len + count)) == nullptr)
                    return nullptr;

                ListConcat(arguments, function->currying);

                for (size_t i = 0; i < count; i++)
                    ListAppend(arguments, args[i]);

                args = arguments->objects;
                count = arguments->len;
            } else {
                args = function->currying->objects;
                count = function->currying->len;
            }
        }
    }

    ret = function->native_fn(instance, args, count);
    Release(arguments);

    return ret;
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

struct CallHelper {
    Frame *frame;
    Function *func;
    List *list_params;
    ArObject **params;

    argon::lang::OpCodeCallFlags flags;

    unsigned short stack_offset;
    unsigned short local_args;
    unsigned short total_args;
};

bool CallSpreadExpansion(CallHelper *helper) {
    ArObject **stack = helper->frame->eval_stack - helper->local_args;

    if ((helper->list_params = ListNew(helper->local_args)) != nullptr) {
        for (int i = 0; i < helper->local_args - 1; i++)
            ListAppend(helper->list_params, stack[i]);

        if (!ListConcat(helper->list_params, stack[helper->local_args - 1])) {
            Release(helper->list_params);
            return false;
        }

        // Update locals_args
        helper->local_args = helper->list_params->len;
        helper->params = helper->list_params->objects;

        return true;
    }

    return false;
}

bool PrepareCall(CallHelper *helper, Frame *frame) {
    helper->stack_offset = argon::lang::I32Arg(frame->instr_ptr);
    helper->flags = (argon::lang::OpCodeCallFlags) argon::lang::I32ExtractFlag(frame->instr_ptr);
    helper->frame = frame;

    helper->func = (Function *) *(frame->eval_stack - helper->stack_offset - 1);

    helper->local_args = helper->stack_offset;

    if (AR_TYPEOF(helper->func, type_function_)) {
        if (helper->local_args > 0 && *(frame->eval_stack - helper->local_args) == nullptr)
            helper->local_args--;

        helper->params = frame->eval_stack - helper->local_args;

        if (ENUMBITMASK_ISTRUE(helper->flags, argon::lang::OpCodeCallFlags::SPREAD))
            if (!CallSpreadExpansion(helper))
                return false;

        helper->total_args = helper->local_args;

        if (helper->func->currying != nullptr)
            helper->total_args += helper->func->currying->len;

        return true;
    }

    ErrorFormat(&error_type_error, "'%s' object is not callable", AR_TYPE_NAME(helper->func));
    return false;
}

void ClearCall(CallHelper *helper) {
    for (size_t i = helper->stack_offset + 1; i > 0; i--)
        Release(*(--helper->frame->eval_stack));

    Release(helper->list_params);
}

inline bool IsPartialApplication(CallHelper *helper) { return helper->total_args < helper->func->arity; }

ArObject *MkCurrying(CallHelper *helper) {
    List *currying;
    ArObject *ret;

    if (helper->func->arity > 0 && helper->total_args == 0) {
        ErrorFormat(&error_type_error, "%s() takes %d argument, but 0 were given",
                    helper->func->name->buffer, helper->func->arity);
        ClearCall(helper);
        return nullptr;
    }

    if ((currying = ListNew(helper->local_args)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < helper->local_args; i++) {
        if (!ListAppend(currying, helper->params[i])) {
            Release(currying);
            return nullptr;
        }
    }

    ret = FunctionNew(helper->func, currying);
    Release(currying);
    ClearCall(helper);

    return ret;
}

bool CheckVariadic(CallHelper *helper) {
    unsigned short exceeded = helper->total_args - helper->func->arity;
    ArObject *ret;

    if (helper->total_args > helper->func->arity) {
        if (!helper->func->IsVariadic()) {
            ErrorFormat(&error_type_error, "%s() takes %d argument, but %d were given",
                        helper->func->name->buffer, helper->func->arity, helper->total_args);
            ClearCall(helper);
            return false;
        }

        if (!helper->func->IsNative()) {
            ret = RestElementToList(helper->params + (helper->local_args - exceeded), exceeded);

            if (ret == nullptr) {
                ClearCall(helper);
                return false;
            }

            Release(helper->params[(helper->local_args - exceeded)]);
            helper->params[(helper->local_args - exceeded)] = ret;
            helper->local_args -= exceeded - 1;
        }
    }

    return true;
}

bool ExecDefer(ArRoutine *routine) {
    Function *func;
    Frame *frame;

    while (routine->defer != nullptr && routine->defer->frame == routine->frame) {
        func = (Function *) routine->defer->function;
        routine->cu_defer = routine->defer;

        if (!func->IsNative()) {
            // TODO: check proxy_globals
            if ((frame = FrameNew(func->code, func->gns, nullptr)) == nullptr)
                return false;

            FrameFillForCall(frame, func, nullptr, 0);

            frame->back = routine->frame;
            routine->frame = frame;
            return true;
        }

        Release(NativeCall(func, nullptr, 0));
        RoutinePopDefer(routine);
    }

    return false;
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
#define TOP_BACK()  (*(--cu_frame->eval_stack))
#define TOP_REPLACE(obj)                \
    Release(*(cu_frame->eval_stack-1)); \
    *(cu_frame->eval_stack-1)=obj
#define PEEK1()     (*(cu_frame->eval_stack-2))
#define PEEK2()     (*(cu_frame->eval_stack-3))

#define BINARY_OP(routine, op, opchar)                                                          \
    if ((ret = Binary(routine, PEEK1(), TOP(), offsetof(OpSlots, op))) == nullptr) {            \
        if (!RoutineIsPanicking(routine)) {                                                     \
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
    ArObject *last_popped = NilVal;
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
                CallHelper helper{};
                Frame *fn_frame;

                if (!PrepareCall(&helper, cu_frame))
                    goto error;

                if (IsPartialApplication(&helper)) {
                    if ((ret = MkCurrying(&helper)) == nullptr)
                        goto error;

                    PUSH(ret);
                    DISPATCH4();
                }

                if (!CheckVariadic(&helper))
                    goto error;

                if (helper.func->IsNative()) {
                    ret = NativeCall(helper.func, helper.params, helper.local_args);
                    ClearCall(&helper);

                    if (ret == nullptr)
                        goto error;

                    PUSH(ret);
                    DISPATCH4();
                }

                // Argon Code
                // TODO: check proxy_globals
                if ((fn_frame = FrameNew(helper.func->code, helper.func->gns, nullptr)) == nullptr) {
                    ClearCall(&helper);
                    goto error;
                }

                FrameFillForCall(fn_frame, helper.func, helper.params, helper.local_args);
                ClearCall(&helper);

                // Invoke:
                cu_frame->instr_ptr += sizeof(argon::lang::Instr32);
                fn_frame->back = cu_frame;
                routine->frame = fn_frame;
                goto begin;
            }
            TARGET_OP(CMP) {
                if ((ret = RichCompare(PEEK1(), TOP(), (CompareMode) ARG16)) == nullptr)
                    goto error;

                POP();
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(DEC) {
                UNARY_OP(dec);
            }
            TARGET_OP(DFR) {
                CallHelper helper{};

                if (!PrepareCall(&helper, cu_frame))
                    goto error;

                if (IsPartialApplication(&helper)) {
                    ErrorFormat(&error_type_error, "%s() takes %d argument, but %d were given",
                                helper.func->name->buffer, helper.func->arity, helper.total_args);
                    ClearCall(&helper);
                    goto error;
                }

                if (!CheckVariadic(&helper))
                    goto error;

                if (helper.list_params != nullptr) {
                    ret = FunctionNew(helper.func, helper.list_params);
                    ClearCall(&helper);
                } else
                    ret = MkCurrying(&helper);

                if (ret == nullptr)
                    goto error;

                RoutineNewDefer(routine, ret);
                Release(ret);
                DISPATCH4();
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

                if ((ret = PropertyGet(TOP(), attribute, false)) == nullptr) {
                    Release(attribute);
                    goto error;
                }

                Release(attribute);
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(IMPMOD) {
                auto path = (String *) TupleGetItem(cu_code->statics, ARG32);

                if ((ret = ::ImportModule(routine, path)) == nullptr) {
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
                BINARY_OP(routine, inp_mul, *=);
            }
            TARGET_OP(IPSUB) {
                BINARY_OP(routine, inp_sub, -=);
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

                if ((ret = PropertyGet(TOP(), key, true)) == nullptr) {
                    Release(key);
                    goto error;
                }

                Release(key);

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(LDENC) {
                if ((ret = ListGetItem(cu_frame->enclosed, ARG16)) == nullptr)
                    goto error;

                PUSH(ret);
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
            TARGET_OP(LDITER) {
                if ((ret = IteratorGet(TOP())) == nullptr)
                    goto error;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(LDLC) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                IncRef(cu_frame->locals[idx]);
                PUSH(cu_frame->locals[idx]);
                DISPATCH2();
            }
            TARGET_OP(LDMETH) {
                // TODO: CHECK OutOfBound
                ArObject *key = TupleGetItem(cu_code->statics, ARG32);
                ArObject *instance = TOP();
                bool meth_found;

                ret = InstanceGetMethod(instance, key, &meth_found);
                Release(key);

                if (ret == nullptr)
                    goto error;

                if (meth_found) {
                    // |method|self|arg1|arg2|....
                    *(cu_frame->eval_stack - 1) = ret;
                    PUSH(instance);
                } else {
                    // |method|nullptr|arg1|arg2|....
                    TOP_REPLACE(ret);
                    PUSH(nullptr);
                }

                DISPATCH4();
            }
            TARGET_OP(LDSCOPE) {
                // TODO: CHECK OutOfBound
                ArObject *key = TupleGetItem(cu_code->statics, ARG32);

                if ((ret = PropertyGet(TOP(), key, false)) == nullptr) {
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
                auto args = ARG16;
                ArObject *step = nullptr;
                ArObject *stop = nullptr;

                if (args == 3) {
                    step = TOP_BACK();
                }

                if (args >= 2) {
                    stop = TOP_BACK();
                }

                ret = BoundsNew(TOP(), stop, step);
                Release(step);
                Release(stop);

                if (ret == nullptr)
                    goto error;

                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_FUNC) {
                auto flags = (FunctionType) argon::lang::I32ExtractFlag(cu_frame->instr_ptr);
                auto name = (String *) PEEK1();
                List *enclosed = nullptr;

                ret = TOP();

                if (ENUMBITMASK_ISTRUE(flags, FunctionType::CLOSURE)) {
                    enclosed = (List *) TOP();
                    ret = PEEK1();
                    name = (String *) PEEK2();
                }

                ret = FunctionNew(cu_frame->globals, name, nullptr, (Code *) ret, enclosed, ARG16, flags);

                if (ret == nullptr)
                    goto error;

                if (ENUMBITMASK_ISTRUE(flags, FunctionType::CLOSURE))
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
                auto args = ARG32;

                if ((ret = SetNew()) == nullptr)
                    goto error;

                // Fill set
                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    if (!SetAdd((Set *) ret, *cursor))
                        goto error;
                    Release(*cursor);
                }

                cu_frame->eval_stack -= args;
                PUSH(ret);
                DISPATCH4();
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
            TARGET_OP(NJE) {
                if ((ret = IteratorNext(TOP())) == nullptr) {
                    if (RoutineIsPanicking(routine))
                        goto error;

                    POP();
                    JUMPTO(ARG32);
                }

                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(NGV) {
                // TODO: CHECK OutOfBound
                auto map = cu_frame->proxy_globals != nullptr ?
                           cu_frame->proxy_globals : cu_frame->globals;

                ret = TupleGetItem(cu_code->names, ARG16);

                if (!NamespaceNewSymbol(map, ret, TOP(), PropertyInfo((PropertyType) (ARG32 >> (unsigned char) 16))))
                    goto error;

                Release(ret);
                POP();
                DISPATCH4();
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
                if (cu_frame->stack_extra_base != cu_frame->eval_stack)
                    cu_frame->return_value = TOP_BACK();

                goto end_function;
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

                if (!PropertySet(TOP(), key, PEEK1(), true)) {
                    Release(key);
                    goto error;
                }

                Release(key);
                POP(); // Value
                POP(); // Instance
                DISPATCH4();
            }
            TARGET_OP(STENC) {
                if (!ListSetItem(cu_frame->enclosed, TOP(), ARG16))
                    goto error;

                POP();
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

                if (!PropertySet(TOP(), key, PEEK1(), false)) {
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

                if (!AsSequence(ret)) {
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

    end_function:

    assert(cu_frame->stack_extra_base == cu_frame->eval_stack);

    // Disabled frame stack to prevent a deferred function from writing a return value.
    cu_frame->eval_stack = nullptr;

    while (true) {
        if (ExecDefer(routine))
            goto begin;

        if (routine->frame->back == nullptr || routine->frame == first_frame)
            break;

        routine->frame = cu_frame->back;

        if (routine->frame->eval_stack != nullptr) {
            ret = NilVal;

            if (cu_frame->return_value != nullptr)
                ret = cu_frame->return_value;

            *routine->frame->eval_stack = IncRef(ret);
            routine->frame->eval_stack++;
            FrameDel(cu_frame);

            if (RoutineIsPanicking(routine)) {
                cu_frame = routine->frame;  // set cu_frame before jump to error
                goto error;
            }

            goto begin;
        }

        cu_frame = routine->frame;  // set cu_frame before execute another defer
        RoutinePopDefer(routine);
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
#undef TOP_BACK
#undef TOP_REPLACE
#undef PEEK1
#undef PEEK2

#undef BINARY_OP
#undef UNARY_OP
}