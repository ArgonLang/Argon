// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>
#include <object/datatype/error.h>

#include "basicblock.h"

using namespace argon::lang::compiler;

BasicBlock *argon::lang::compiler::BasicBlockNew() {
    auto *bb = (BasicBlock *) argon::memory::Alloc(sizeof(BasicBlock));

    if (bb == nullptr) {
        argon::vm::Panic(argon::object::error_out_of_memory);
        return nullptr;
    }

    argon::memory::MemoryZero(bb, sizeof(BasicBlock));
    return bb;
}

BasicBlock *argon::lang::compiler::BasicBlockDel(BasicBlock *block) {
    BasicBlock *prev = block->next;
    Instr *tmp;

    // Cleanup instr
    for (Instr *cur = block->instr.head; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        argon::memory::Free(cur);
    }

    argon::memory::Free(block);
    return prev;
}

Instr *argon::lang::compiler::BasicBlockAddInstr(BasicBlock *block, OpCodes op, int arg) {
    Instr *instr;
    unsigned short op_size;

    switch (op) {
        case OpCodes::LSTATIC:
            op_size = 4;
            break;
        case OpCodes::CMP:
            op_size = 2;
            break;
        default:
            op_size = 1;
    }

    block->i_size = op_size;

    if ((instr = (Instr *) argon::memory::Alloc(sizeof(Instr))) != nullptr) {
        argon::memory::MemoryZero(instr, sizeof(Instr));

        if (block->instr.head == nullptr) {
            block->instr.head = instr;
            block->instr.tail = instr;
        } else {
            block->instr.tail->next = instr;
            block->instr.tail = instr;
        }

        instr->opcode = (unsigned char) op;
        instr->oparg = op_size << 3 | arg; // | instr_len | argument |
    } else
        argon::vm::Panic(argon::object::error_out_of_memory);

    return instr;
}