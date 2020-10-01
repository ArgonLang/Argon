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

    Frame *cu_frame = routine->frame;
    Code *cu_code = cu_frame->code;

    ArObject *ret = nullptr;

    while (cu_frame->instr_ptr < (cu_code->instr + cu_code->instr_sz)) {
        switch (*(argon::lang::OpCodes *) cu_frame->instr_ptr) {
            TARGET_OP(ADD) {
                BINARY_OP(routine, add, +);
            }
            TARGET_OP(CALL) {
                break;
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
                auto args = ARG32;

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
            TARGET_OP(SHL) {
                BINARY_OP(routine, shl, >>);
            }
            TARGET_OP(SHR) {
                BINARY_OP(routine, shr, <<);
            }
            TARGET_OP(STLC) {
                // TODO: CHECK OutOfBound
                auto idx = ARG16;

                Release(cu_frame->locals[idx]);
                cu_frame->locals[idx] = TOP();
                cu_frame->eval_stack--;
                DISPATCH();
            }
            TARGET_OP(SUB) {
                BINARY_OP(routine, sub, -);
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

    return nullptr;

#undef TARGET_OP
#undef DISPATCH
#undef DISPATCH2
#undef DISPATCH4
#undef JUMPTO

#undef ARG16
#undef ARG32

#undef PUSH
#undef POP
#undef TOP
#undef TOP_REPLACE
#undef PEEK1
#undef PEEK2

#undef BINARY_OP
#undef UNARY_OP
}