// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "opcode.h"
#include "areval.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

ArObject *argon::vm::Eval(Fiber *fiber) {
#ifndef ARGON_FF_COMPUTED_GOTO
#define TARGET_OP(op) case OpCode::op:

#define CGOTO   continue
#endif

// STACK MANIPULATION MACRO
#define POP()   Release(*(--cu_frame->eval_stack))

#define PUSH(obj) do {              \
    *cu_frame->eval_stack = obj;    \
    cu_frame->eval_stack++;         \
    } while(0)

#define TOP()   (*(cu_frame->eval_stack - 1))

#define DISPATCH()                                              \
    cu_frame->instr_ptr += OpCodeOffset[*cu_frame->instr_ptr];  \
    CGOTO

    Frame *cu_frame = fiber->frame;
    const Code *cu_code = cu_frame->code;

    while (cu_frame->instr_ptr < cu_code->instr_end) {
        switch (*((OpCode *) cu_frame->instr_ptr)) {

        }
    }

    return nullptr;
}
