// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "argon_vm.h"
#include <lang/opcodes.h>
#include <object/bool.h>
#include <object/error.h>

using namespace lang;
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

#define TOP()       frame->eval_stack[es_cur-1]
#define PEEK1()     frame->eval_stack[es_cur-2]

#define TOP_REPLACE(obj)                \
Release(frame->eval_stack[es_cur-1]);   \
frame->eval_stack[es_cur-1]=obj

#define GET_ACTIONS(struct, offset) *((BinaryOp *) (((unsigned char *) struct) + offset))

ArObject *Binary(ArRoutine *routine, ArObject *l, ArObject *r, int op) {
    BinaryOp lop = GET_ACTIONS(l->type, op);
    BinaryOp rop = GET_ACTIONS(r->type, op);
    ArObject *result = nullptr;

    if (lop != nullptr)
        result = lop(l, r);
    if (rop != nullptr && result == NotImpl) {
        Release(result);
        result = rop(l, r);
    }

    if (result == NotImpl) {
        // TODO: SET ROUTINE ERROR
        result = nullptr;
    }

    return result;
}

void ArgonVM::Eval(ArRoutine *routine) {
    Frame *frame = routine->frame;
    object::Code *code = frame->code;
    unsigned int es_cur = ((unsigned char *) frame->eval_stack) - (unsigned char *) frame->stack_extra_base;

    ArObject *ret = nullptr;

    while (frame->instr_ptr < (code->instr + code->instr_sz)) {
        switch (*(lang::OpCodes *) frame->instr_ptr) {
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
            TARGET_OP(JMP) {
                JUMPTO(I32Arg(frame->instr_ptr));
            }
            TARGET_OP(JF) {
                if (!IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(frame->instr_ptr));
                }
                POP();
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
            TARGET_OP(JFOP) {
                if (IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(I32Arg(frame->instr_ptr));
            }
            TARGET_OP(ADD) {
                ret = Binary(routine, PEEK1(), TOP(), offsetof(TypeInfo, add));
                if (ret == nullptr)
                    goto error;
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(SUB) {
                ret = Binary(routine, PEEK1(), TOP(), offsetof(TypeInfo, sub));
                if (ret == nullptr)
                    goto error;
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MUL) {
                ret = Binary(routine, PEEK1(), TOP(), offsetof(TypeInfo, mul));
                if (ret == nullptr)
                    goto error;
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(DIV) {
                ret = Binary(routine, PEEK1(), TOP(), offsetof(TypeInfo, div));
                if (ret == nullptr)
                    goto error;
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(LSTATIC) {
                PUSH(TupleGetItem(code->statics, I16Arg(frame->instr_ptr)));
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
                POP();
                DISPATCH();
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
            default:
                assert(false);
        }
        error:
        assert(false);
    }
    assert(es_cur == 0);
}

