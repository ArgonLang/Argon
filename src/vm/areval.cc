// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/nil.h>
#include <object/datatype/list.h>
#include <object/datatype/tuple.h>
#include <object/datatype/bounds.h>
#include <object/datatype/map.h>
#include <object/datatype/function.h>
#include <object/objmgmt.h>

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
    if (rop != nullptr && result == nullptr && routine->panic_object == nullptr) {
        Release(result);
        result = rop(l, r);
    }

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
                    Release(arguments);
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
        if (RoutineIsPanicking(routine)) {                                                      \
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
    ArObject *last_popped = nullptr;

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
                        // TODO error
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
                        //ErrorFormat(error_type_error, "", func->) // TODO ERROR, function name!
                        goto error;
                    }

                    if ((ret = RestElementToList(cu_frame->eval_stack - exceeded, exceeded)) == nullptr)
                        goto error;

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
                auto func_frame = FrameNew(func->code, cu_frame->globals, cu_frame->proxy_globals);
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
            }
            TARGET_OP(IMPMOD) {
            }
            TARGET_OP(INC) {
                UNARY_OP(inc);
            }
            TARGET_OP(INIT) {

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
            TARGET_OP(LDGBL) {
                // TODO: CHECK OutOfBound
                auto *key = TupleGetItem(cu_code->names, ARG16);

                ret = NamespaceGetValue(cu_frame->globals, key, nullptr);

                if (ret == nullptr && cu_frame->proxy_globals != nullptr)
                    ret = NamespaceGetValue(cu_frame->proxy_globals, key, nullptr);

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
                auto flags = (argon::lang::MkFuncFlags) ARG32;
                List *enclosed = nullptr;

                ret = TOP();

                if (ENUMBITMASK_ISTRUE(flags, argon::lang::MkFuncFlags::CLOSURE)) {
                    enclosed = (List *) TOP();
                    ret = PEEK1();
                }

                ret = FunctionNew((Code *) ret, ARG16,
                                  ENUMBITMASK_ISTRUE(flags, argon::lang::MkFuncFlags::VARIADIC),
                                  enclosed);

                if (ret == nullptr)
                    goto error;

                if (ENUMBITMASK_ISTRUE(flags, argon::lang::MkFuncFlags::CLOSURE))
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

            }
            TARGET_OP(MK_TRAIT) {

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

                if (!NamespaceNewSymbol(map, PropertyInfo(ARG32 >> (unsigned char) 16), ret, TOP()))
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
            TARGET_OP(RET) {
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
                DISPATCH();
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
        }
        error:
        Release(ret);
        assert(false);
    }

    end_while:

    assert(cu_frame->stack_extra_base == cu_frame->eval_stack);

    if (cu_frame->back != nullptr) {
        routine->frame = cu_frame->back;
        FrameDel(cu_frame);
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