// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "argon_vm.h"
#include <lang/opcodes.h>

using namespace lang;
using namespace argon::vm;

#ifndef COMPUTED_GOTO
#define TARGET_OP(op) case OpCodes::op:
#define DISPATCH() continue
#else
#define TARGET_OP(op)    \
    case OpCodes::op:   \
    LBL_##op:

#define DISPATCH goto *computed_goto[(unsigned char)*(lang::OpCodes *) frame->instr_ptr]
#endif

#define IP_NEXT()   frame->instr_ptr+=sizeof(Instr8)
#define IP_NEXT2()  frame->instr_ptr+=sizeof(Instr16)
#define IP_NEXT4()  frame->instr_ptr+=sizeof(Instr32)

#define PUSH(obj) frame->eval_stack[es_cur++] = obj

void ArgonVM::Eval(ArRoutine *routine) {
    Frame *frame = routine->frame;
    object::Code *code = frame->code;
    unsigned int es_cur = frame->eval_index;

    for (;;) {
        switch (*(lang::OpCodes *) frame->instr_ptr) {
            TARGET_OP(ADD) {
                IP_NEXT();
                DISPATCH();
            }
            TARGET_OP(LSTATIC) {
//                object::Object *value = code->statics->GetItem(I16Arg(frame->instr_ptr));
                //              object::IncStrongRef(value);
                //            PUSH(value);
                IP_NEXT2();
                DISPATCH();
            }
            default:
                assert(false);
        }
    }
}

