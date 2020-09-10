// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "areval.h"
#include <lang/opcodes.h>
#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/map.h>
#include <object/datatype/function.h>
#include <object/datatype/nil.h>
#include <object/datatype/bounds.h>
#include <object/datatype/trait.h>
#include <object/datatype/struct.h>
#include <object/datatype/instance.h>

using namespace argon::lang;
using namespace argon::object;
using namespace argon::vm;

#ifndef COMPUTED_GOTO
#define TARGET_OP(op) case OpCodes::op:

#define DISPATCH()                  \
frame->instr_ptr+=sizeof(Instr8);   \
continue

#define DISPATCH2()                 \
frame->instr_ptr+=sizeof(Instr16);  \
continue

#define DISPATCH4()                 \
frame->instr_ptr+=sizeof(Instr32);  \
continue

#define JUMPTO(offset)                                      \
frame->instr_ptr = (unsigned char *) code->instr + offset;  \
continue

#else
#define TARGET_OP(op)    \
    case OpCodes::op:   \
    LBL_##op:

#define DISPATCH goto *computed_goto[(unsigned char)*(lang::OpCodes *) frame->instr_ptr]
#endif

// STACK MANIPULATION MACRO
#define PUSH(obj)   frame->eval_stack[es_cur++] = obj
#define POP()       Release(frame->eval_stack[--es_cur])

#define STACK_REWIND(offset)                                                        \
for (size_t i = es_cur; i > es_cur - (offset); Release(frame->eval_stack[--i]));    \
es_cur -= offset

#define TOP()       frame->eval_stack[es_cur-1]
#define PEEK1()     frame->eval_stack[es_cur-2]
#define PEEK2()     frame->eval_stack[es_cur-3]

#define TOP_REPLACE(obj)                \
Release(frame->eval_stack[es_cur-1]);   \
frame->eval_stack[es_cur-1]=obj

#define GET_BINARY_OP(struct, offset) *((BinaryOp *) (((unsigned char *) struct) + offset))

ArObject *Binary(ArRoutine *routine, ArObject *l, ArObject *r, int offset) {
    BinaryOp lop = GET_BINARY_OP(l->type->ops, offset);
    BinaryOp rop = GET_BINARY_OP(r->type->ops, offset);

    ArObject *result = nullptr;

    if (lop != nullptr)
        result = lop(l, r);
    if (rop != nullptr && result == nullptr && routine->panic_object == nullptr) {
        Release(result);
        result = rop(l, r);
    }

    return result;
}

#define BINARY_OP(routine, op, opchar)                                                      \
if ((ret = Binary(routine, PEEK1(), TOP(), offsetof(OpSlots, op))) == nullptr) {            \
    if(routine->panic_object==nullptr)                                                      \
        ErrorFormat(&error_type_error, "unsupported operand type '%s' for: '%s' and '%s'",  \
        #opchar, PEEK1()->type->name, TOP()->type->name);                                   \
    goto error;                                                                             \
    }                                                                                       \
POP();                                                                                      \
TOP_REPLACE(ret);                                                                           \
DISPATCH()

bool GetMRO(ArRoutine *routine, List **mro, Trait **impls, size_t count) {
    (*mro) = nullptr;

    if (count > 0) {
        List *bases = BuildBasesList((Trait **) impls, count);
        if (bases == nullptr) {
            assert(false);
            return false; // TODO memerror!
        }

        (*mro) = ComputeMRO(bases);
        Release(bases);

        if ((*mro) == nullptr) {
            assert(false);
            return false; // TODO memerror!
        }

        if ((*mro)->len == 0) {
            assert(false);
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

    if (code->type != &type_code_)
        return nullptr; // todo: Expected Code object!

    if ((ns = NamespaceNew()) == nullptr)
        return nullptr; // TODO: enomem

    if ((frame = FrameNew(code, routine->frame->globals, ns)) == nullptr) {
        Release(ns);
        return nullptr; // TODO: enomem
    }

    Eval(routine, frame);
    FrameDel(frame);

    if (!GetMRO(routine, &mro, impls, count)) {
        Release(ns);
        return nullptr; // TODO mro error
    }

    ret = is_trait ? (ArObject *) TraitNew(name, ns, mro) : (ArObject *) StructNew(name, ns, mro);
    Release(ns);
    Release(mro);

    if (ret == nullptr) {
        // todo:enomem
    }

    return ret;
}

ArObject *InstantiateStruct(ArRoutine *routine, Struct *base, ArObject **values, size_t count, bool key_pair) {
    Instance *instance;
    Namespace *instance_ns;

    if ((instance_ns = NamespaceNew()) == nullptr)
        return nullptr; // todo: enomem

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
                    return nullptr; // todo: enomem
                }
            }
        }
    } else {
        // Load default values
        for (NsEntry *cur = base->names->iter_begin; cur != nullptr; cur = cur->iter_next) {
            if (cur->info.IsMember())
                if (!NamespaceNewSymbol(instance_ns, cur->info, cur->key, cur->obj)) {
                    Release(instance_ns);
                    return nullptr; // todo: enomem
                }
        }

        for (size_t i = 0; i < count; i += 2) {
            if (!NamespaceSetValue(instance_ns, values[i], values[i + 1])) {
                Release(instance_ns);
                return nullptr; // todo: Invalid Key!
            }
        }
    }

    instance = InstanceNew(base, instance_ns);
    Release(instance_ns);

    // todo: set enomem if nullptr

    return instance;
}

void FillFrameBeforeCall(Frame *frame, Function *callable, ArObject **args, size_t count) {
    size_t local_idx = 0;

    // If method, first parameter must be an instance
    if (callable->instance != nullptr) {
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

ArObject *NativeCall(ArRoutine *routine, Function *function, ArObject **args, size_t count) {
    List *arguments = nullptr;
    ArObject **raw = nullptr;

    if (function->arity > 0) {
        raw = args;

        if (count < function->arity) {
            if ((arguments = ListNew(function->arity)) == nullptr) {
                return nullptr; // TODO: enomem
            }

            if (function->currying != nullptr) {
                if (!ListConcat(arguments, function->currying)) {
                    return nullptr; // TODO: enomem
                }
            }

            for (size_t i = 0; i < count; i++)
                ListAppend(arguments, args[i]);

            if (function->variadic && arguments->len + 1 == function->arity)
                ListAppend(arguments, NilVal);

            raw = arguments->objects;
        }
    }

    auto res = function->native_fn(function, raw);
    Release(arguments);

    return res;
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
    Frame *base = routine->frame; // TODO: refactor THIS!
    ArObject *last_pop = nullptr;

    begin:
    Frame *frame = routine->frame;
    object::Code *code = frame->code;
    unsigned int es_cur =
            (((unsigned char *) frame->eval_stack) - (unsigned char *) frame->stack_extra_base) / sizeof(ArObject *);
    frame->eval_stack = (ArObject **) frame->stack_extra_base;

    ArObject *ret = nullptr;

    while (frame->instr_ptr < (code->instr + code->instr_sz)) {
        switch (*(lang::OpCodes *) frame->instr_ptr) {

            TARGET_OP(CALL) {
                unsigned short local_args = I16Arg(frame->instr_ptr);
                unsigned short total_args = local_args;
                auto func = (Function *) frame->eval_stack[es_cur - local_args - 1];
                unsigned short arity = func->arity;

                if (func->type != &type_function_)
                    goto error; //TODO: not callable!

                if (func->instance != nullptr) total_args++;

                if (func->currying != nullptr) total_args += func->currying->len;

                if (func->variadic) arity--;

                if (total_args < arity) {
                    if (total_args == 0)
                        goto error; // TODO: set error MISSING PARAMS

                    if ((ret = MkCurrying(func, frame->eval_stack + (es_cur - local_args), local_args)) == nullptr)
                        goto error; // TODO: enomem

                    STACK_REWIND(local_args);
                    TOP_REPLACE(ret);
                    DISPATCH2();
                } else if (total_args > arity) {
                    unsigned short exceeded = total_args - arity;

                    if (!func->variadic)
                        goto error; // TODO: Too much arguments!

                    ret = RestElementToList(frame->eval_stack + (es_cur - (exceeded)), exceeded);

                    if (ret == nullptr)
                        goto error; // TODO: enomem

                    STACK_REWIND(exceeded - 1);
                    TOP_REPLACE(ret);
                }

                if (func->native) {
                    ret = NativeCall(routine, func, frame->eval_stack + (es_cur - local_args), local_args);

                    if (ret == nullptr)
                        goto error;

                    STACK_REWIND(local_args);
                    TOP_REPLACE(ret);
                    DISPATCH2();
                }

                auto fr = FrameNew(func->code, frame->globals, frame->proxy_globals);
                if (fr == nullptr)
                    goto error; // TODO: enomem

                FillFrameBeforeCall(fr, func, frame->eval_stack + (es_cur - local_args), local_args);
                es_cur -= local_args;

                assert(TOP()->type == &type_function_);
                POP(); // pop function!

                // Invoke function!
                // save PC
                frame->instr_ptr += sizeof(Instr16);
                frame->eval_stack = &frame->eval_stack[es_cur];
                fr->back = frame;
                routine->frame = fr;
                goto begin;
            }

            TARGET_OP(CMP) {
                auto mode = (CompareMode) I16Arg(frame->instr_ptr);
                ArObject *left = PEEK1();
                if (mode == CompareMode::EQ)
                    ret = BoolToArBool(left->type->equal(left, TOP()));
                else if (mode == CompareMode::NE)
                    ret = BoolToArBool(!left->type->equal(left, TOP()));
                else {
                    if (left->type->compare != nullptr)
                        ret = left->type->compare(left, TOP(), mode);
                    else
                        assert(false); // TODO: not impl error!
                }
                POP();
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(TEST) {
                ArObject *left = PEEK1();
                if (left->type->equal(left, TOP())) {
                    POP();
                    TOP_REPLACE(BoolToArBool(true));
                    DISPATCH();
                }
                TOP_REPLACE(BoolToArBool(false));
                DISPATCH();
            }
            TARGET_OP(NOT) {
                ret = True;
                if (IsTrue(TOP()))
                    ret = False;
                IncRef(ret);
                TOP_REPLACE(ret);
                DISPATCH();
            }

                // *** JUMP ***

            TARGET_OP(JF) {
                if (!IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(frame->instr_ptr));
                }
                POP();
                DISPATCH4();
            }
            TARGET_OP(JFOP) {
                if (IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(I32Arg(frame->instr_ptr));
            }
            TARGET_OP(JMP) {
                JUMPTO(I32Arg(frame->instr_ptr));
            }
            TARGET_OP(JNIL) {
                if (TOP() == NilVal) {
                    JUMPTO(I32Arg(frame->instr_ptr));
                }
                DISPATCH4();
            }
            TARGET_OP(JT) {
                if (IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(frame->instr_ptr));
                }
                POP();
                DISPATCH4();
            }
            TARGET_OP(JTOP) {
                if (!IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(I32Arg(frame->instr_ptr));
            }

                // *** NEW VARIABLES ***

            TARGET_OP(NGV) {
                auto map = frame->globals;
                assert(code->names != nullptr);
                ret = TupleGetItem(code->names, I16Arg(frame->instr_ptr)); // I16 extract arg bit

                if (frame->proxy_globals != nullptr)
                    map = frame->proxy_globals;

                assert(!NamespaceContains(map, ret, nullptr)); // Double declaration, compiler error!
                if (!NamespaceNewSymbol(map, PropertyInfo(I32Arg(frame->instr_ptr) >> 16), ret, TOP())) {
                    Release(ret);
                    goto error; // todo: memory error!
                }

                Release(ret);
                POP();
                DISPATCH4();
            }
            TARGET_OP(NLV) {
                IncRef(TOP());
                frame->locals[I16Arg(frame->instr_ptr)] = TOP();
                POP();
                DISPATCH2();
            }

                // *** STORE VALUES ***

            TARGET_OP(STATTR) {
                assert(code->statics != nullptr);
                ArObject *key = TupleGetItem(code->statics, I16Arg(frame->instr_ptr));
                ArObject *instance = (Instance *) PEEK1();

                if (!instance->type->obj_actions->set_attr(instance, key, TOP())) {
                    Release(key);
                    goto error; // TODO: key not found / priv,pub ecc
                }

                Release(key);

                POP(); // value
                POP(); // instance
                DISPATCH2();
            }
            TARGET_OP(STGBL) {
                auto map = frame->globals;

                assert(code->names != nullptr);
                PropertyInfo info;
                ret = TupleGetItem(code->names, I16Arg(frame->instr_ptr));

                if (frame->proxy_globals != nullptr)
                    map = frame->proxy_globals;

                if (!NamespaceContains(map, ret, &info)) {
                    Release(ret);
                    goto error; // todo: Unknown variable
                }

                if (info.IsConstant()) {
                    Release(ret);
                    goto error; // todo: Constant!
                }

                NamespaceSetValue(map, ret, TOP());
                Release(ret);
                POP();
                DISPATCH4();
            }
            TARGET_OP(STLC) {
                assert(frame->locals != nullptr);
                Release(frame->locals[I16Arg(frame->instr_ptr)]);
                frame->locals[I16Arg(frame->instr_ptr)] = TOP();
                --es_cur; // back eval pointer without release object!
                DISPATCH2();
            }
            TARGET_OP(STSUBSCR) {
                if (IsMap(PEEK2())) {
                    // todo: check if key is hasbale!
                    if (!PEEK2()->type->map_actions->set_item(PEEK2(), PEEK1(), TOP()))
                        goto error; // todo memory error
                } else if (IsSequence(PEEK2())) {
                    if (AsIndex(PEEK1())) {
                        if (PEEK2()->type->sequence_actions->set_item(PEEK2(), TOP(),
                                                                      TOP()->type->number_actions->as_index(TOP())))
                            goto error; // todo outofbounderror or memoryerror
                    } else if (PEEK1()->type == &type_bounds_) {
                        if (PEEK2()->type->sequence_actions->set_slice(PEEK2(), PEEK1(), TOP()))
                            goto error; // todo outofbounderror or memoryerror
                    } else
                        goto error; // todo: key must be index or bounds!
                } else
                    goto error; // todo: object not subscriptable
                POP();
                POP();
                POP();
                DISPATCH();
            }

                // *** LOAD VARIABLES ***

            TARGET_OP(LDATTR) {
                assert(code->statics != nullptr);
                ArObject *key = TupleGetItem(code->statics, I16Arg(frame->instr_ptr));
                ArObject *instance = (Instance *) TOP();

                ret = instance->type->obj_actions->get_attr(instance, key);
                Release(key);

                if (ret == nullptr)
                    goto error; // TODO: key not found / priv,pub ecc

                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(LDENC) {
                assert(frame->enclosed != nullptr);
                IncRef(frame->enclosed[I16Arg(frame->instr_ptr)]);
                PUSH(frame->enclosed[I16Arg(frame->instr_ptr)]);
                DISPATCH2();
            }
            TARGET_OP(LDGBL) {
                auto map = frame->globals;

                assert(code->names != nullptr);
                ArObject *key = TupleGetItem(code->names, I16Arg(frame->instr_ptr));

                ret = NamespaceGetValue(frame->globals, key, nullptr);

                if (ret == nullptr && frame->proxy_globals != nullptr)
                    ret = NamespaceGetValue(frame->proxy_globals, key, nullptr);

                Release(key);

                if (ret == nullptr)
                    goto error; // todo: Unknown variable

                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(LDLC) {
                assert(frame->locals != nullptr);
                IncRef(frame->locals[I16Arg(frame->instr_ptr)]);
                PUSH(frame->locals[I16Arg(frame->instr_ptr)]);
                DISPATCH2();
            }
            TARGET_OP(LSTATIC) {
                PUSH(TupleGetItem(code->statics, I32Arg(frame->instr_ptr)));
                DISPATCH4();
            }
            TARGET_OP(SUBSCR) {
                if (IsMap(PEEK1())) {
                    // todo: check if key is hasbale!
                    if ((ret = PEEK1()->type->map_actions->get_item(PEEK1(), TOP())) == nullptr)
                        goto error; // todo key not exists
                } else if (IsSequence(PEEK1())) {
                    if (AsIndex(TOP())) {
                        ret = PEEK1()->type->sequence_actions->get_item(PEEK1(),
                                                                        TOP()->type->number_actions->as_index(TOP()));
                        if (ret == nullptr)
                            goto error; // todo outofbounderror
                    } else if (TOP()->type == &type_bounds_)
                        ret = PEEK1()->type->sequence_actions->get_slice(PEEK1(), TOP());
                    else
                        goto error; // todo: key must be index or bounds!
                } else
                    goto error; // todo: object not subscriptable
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }

                // *** COMMON MATH OPERATIONS ***

            TARGET_OP(ADD) {
                BINARY_OP(routine, add, +);
            }
            TARGET_OP(SUB) {
                BINARY_OP(routine, sub, -);
            }
            TARGET_OP(MUL) {
                BINARY_OP(routine, mul, *);
            }
            TARGET_OP(DIV) {
                BINARY_OP(routine, div, /);
            }
            TARGET_OP(IDIV) {
                BINARY_OP(routine, idiv, '//');
            }
            TARGET_OP(MOD) {
                BINARY_OP(routine, module, %);
            }
            TARGET_OP(POS) {
                ret = TOP();
                if ((ret = ret->type->ops->pos(ret)) == nullptr)
                    goto error;
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(NEG) {
                ret = TOP();
                if ((ret = ret->type->ops->neg(ret)) == nullptr)
                    goto error;
                TOP_REPLACE(ret);
                DISPATCH();
            }

                // *** COMMON LOGICAL OPERATIONS ***

            TARGET_OP(LAND) {
                BINARY_OP(routine, l_and, &);
            }
            TARGET_OP(LOR) {
                BINARY_OP(routine, l_or, |);
            }
            TARGET_OP(LXOR) {
                BINARY_OP(routine, l_xor, ^);
            }
            TARGET_OP(SHL) {
                BINARY_OP(routine, shl, <<);
            }
            TARGET_OP(SHR) {
                BINARY_OP(routine, shr, >>);
            }
            TARGET_OP(INV) {
                ret = TOP();
                if ((ret = ret->type->ops->invert(ret)) == nullptr)
                    goto error;
                TOP_REPLACE(ret);
                DISPATCH();
            }

                // *** COMPOUND DATA STRUCTURES ***

            TARGET_OP(MK_BOUNDS) {
                unsigned short args = I16Arg(frame->instr_ptr);
                arsize step = 1;
                arsize stop = 0;
                arsize start;

                if (args == 3) {
                    if (!AsIndex(TOP())) {
                        goto error; // todo: MUST BE A NUMBER!
                    }
                    step = TOP()->type->number_actions->as_index(TOP());
                    POP();
                }

                if (args >= 2) {
                    if (!AsIndex(TOP())) {
                        goto error; // todo: MUST BE A NUMBER!
                    }
                    stop = TOP()->type->number_actions->as_index(TOP());
                    POP();
                }

                if (!AsIndex(TOP())) {
                    goto error; // todo: MUST BE A NUMBER!
                }

                start = TOP()->type->number_actions->as_index(TOP());

                if ((ret = BoundsNew(start, stop, step)) == nullptr)
                    goto error; // todo: memoryerror

                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_FUNC) {
                auto flags = (MkFuncFlags) I32ExtractFlag(frame->instr_ptr);
                Code *fn_code = (Code *) TOP();
                List *enclosed = nullptr;

                if (ENUMBITMASK_ISTRUE(flags, MkFuncFlags::CLOSURE)) {
                    enclosed = (List *) TOP();
                    fn_code = (Code *) PEEK1();
                }

                ret = FunctionNew(fn_code, I16Arg(frame->instr_ptr),
                                  ENUMBITMASK_ISTRUE(flags, MkFuncFlags::VARIADIC), enclosed);

                if (ret == nullptr)
                    goto error; // todo: enomem

                if (ENUMBITMASK_ISTRUE(flags, MkFuncFlags::CLOSURE))
                    POP();

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_LIST) {
                unsigned int args = I32Arg(frame->instr_ptr);

                if ((ret = ListNew()) == nullptr)
                    goto error; // todo: memroyerror

                for (unsigned int i = es_cur - args; i < es_cur; i++) {
                    if (!ListAppend((List *) ret, frame->eval_stack[i]))
                        goto error; // TODO: memoryerror
                    Release(frame->eval_stack[i]);
                }

                es_cur -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_STRUCT) {
                unsigned short args = I16Arg(frame->instr_ptr);

                if ((ret = MkConstruct(routine, (Code *) frame->eval_stack[(es_cur - args) - 2],
                                       (String *) frame->eval_stack[(es_cur - args) - 1],
                                       (Trait **) frame->eval_stack + (es_cur - args),
                                       args, false)) == nullptr)
                    goto error;

                STACK_REWIND(args + 1); // args + 1(name)
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_TRAIT) {
                unsigned short args = I16Arg(frame->instr_ptr);

                if ((ret = MkConstruct(routine, (Code *) frame->eval_stack[(es_cur - args) - 2],
                                       (String *) frame->eval_stack[(es_cur - args) - 1],
                                       (Trait **) frame->eval_stack + (es_cur - args),
                                       args, true)) == nullptr)
                    goto error;

                STACK_REWIND(args + 1); // args + 1(name)
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_TUPLE) {
                unsigned int args = I32Arg(frame->instr_ptr);

                if ((ret = TupleNew(args)) == nullptr)
                    goto error; // todo: memoryerror

                for (unsigned int i = es_cur - args, idx = 0; i < es_cur; i++) {
                    if (!TupleInsertAt((Tuple *) ret, idx++, frame->eval_stack[i]))
                        goto error; // TODO: out of range!
                    Release(frame->eval_stack[i]);
                }

                es_cur -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_MAP) {
                unsigned int args = I32Arg(frame->instr_ptr) * 2;

                if ((ret = MapNew()) == nullptr)
                    goto error; // todo: memoryerrpr

                for (unsigned int i = es_cur - args; i < es_cur; i += 2) {
                    if (!MapInsert((Map *) ret, frame->eval_stack[i], frame->eval_stack[i + 1])) {
                        Release(ret);
                        goto error; // todo: memoryerror or unhasbale key!!
                    }
                    Release(frame->eval_stack[i]);
                    Release(frame->eval_stack[i + 1]);
                }
                es_cur -= args;
                PUSH(ret);
                DISPATCH4();
            }

            TARGET_OP(INIT) {
                unsigned short args = I16Arg(frame->instr_ptr);
                bool key_pair = I32ExtractFlag(frame->instr_ptr);
                auto bstruct = (Struct *) frame->eval_stack[(es_cur - args) - 1];

                if (bstruct->type != &type_struct_)
                    goto error; // todo: expected struct

                if (!key_pair && args > bstruct->properties_count)
                    goto error;  // TODO: too many values

                ret = InstantiateStruct(routine, bstruct, frame->eval_stack + (es_cur - args), args, key_pair);
                if (ret == nullptr)
                    goto error;

                STACK_REWIND(args);
                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(RET) {
                assert(frame->back != nullptr);
                if (es_cur == 0) {
                    IncRef(NilVal);
                    frame->back->eval_stack[0] = NilVal;
                } else {
                    frame->back->eval_stack[0] = TOP();
                    es_cur--;
                }
                frame->back->eval_stack++;
                goto end_while;
            }
            TARGET_OP(POP) {
                Release(last_pop);
                last_pop = frame->eval_stack[--es_cur];
                DISPATCH();
            }
            default:
                assert(false);
        }
        error:
        assert(false);
    }

    end_while:
    assert(es_cur == 0);

    if (frame->back != nullptr && frame != base) {
        routine->frame = frame->back;
        FrameDel(frame);
        goto begin;
    }

    return last_pop;
}

