// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/memory/memory.h>

#include "basicblock.h"

using namespace argon::lang;

BasicBlock *argon::lang::BasicBlockNew() {
    return (BasicBlock *) vm::memory::Calloc(sizeof(BasicBlock));
}

BasicBlock *argon::lang::BasicBlockDel(BasicBlock *block) {
    BasicBlock *prev;

    if (block == nullptr)
        return nullptr;

    prev = block->next;

    // Cleanup instr
    Instr *tmp;
    for (Instr *cur = block->instr.head; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        argon::vm::memory::Free(cur);
    }

    argon::vm::memory::Free(block);

    return prev;
}

Instr *BasicBlock::AddInstr(vm::OpCode opcode, int arg) {
    auto *i = (Instr *) argon::vm::memory::Alloc(sizeof(Instr));

    if (i != nullptr) {
        i->next = nullptr;

        assert(((unsigned char) opcode) < sizeof(vm::StackChange));

        auto op_size = vm::OpCodeOffset[(unsigned char) opcode];

        this->size += op_size;

        if (this->instr.head == nullptr) {
            this->instr.head = i;
            this->instr.tail = i;
        } else {
            this->instr.tail->next = i;
            this->instr.tail = i;
        }

        i->opcode = (unsigned char) opcode;
        i->oparg = op_size << 24u | arg; // | instr_len | argument |

        i->jmp = nullptr;
        i->lineno = 0;
    }

    return i;
}