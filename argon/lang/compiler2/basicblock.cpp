// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <argon/vm/memory/memory.h>

#include <argon/lang/compiler2/basicblock.h>

using namespace argon::vm::memory;
using namespace argon::lang::compiler2;

Instr *BasicBlock::AddInstr(vm::OpCode opcode, int arg, unsigned int lineno) {
    auto *i = (Instr *) Alloc(sizeof(Instr));

    if (i != nullptr) {
        i->next = nullptr;
        i->jmp = nullptr;

        this->size += vm::OpCodeOffset[(unsigned char) opcode];

        if (this->instr.head == nullptr) {
            this->instr.head = i;
            this->instr.tail = i;
        } else {
            this->instr.tail->next = i;
            this->instr.tail = i;
        }

        i->opcode = (unsigned char) opcode;
        i->oparg = arg;

        i->lineno = lineno;
    }

    return i;
}

BasicBlock *argon::lang::compiler2::BasicBlockNew() {
    return (BasicBlock *) Calloc(sizeof(BasicBlock));
}

BasicBlock *argon::lang::compiler2::BasicBlockDel(BasicBlock *block) {
    if (block == nullptr)
        return nullptr;

    auto *next = block->next;

    // Cleanup instructions
    for (Instr *tmp, *cur = block->instr.head; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        Free(cur);
    }

    Free(block);

    return next;
}

BasicBlock *BasicBlockSeq::BlockNewAppend() {
    auto *block = BasicBlockNew();
    if (block == nullptr)
        return nullptr;

    this->Append(block);

    return block;
}

bool BasicBlockSeq::CheckLastInstr(vm::OpCode opcode) {
    return this->current->instr.tail != nullptr
           && this->current->instr.tail->opcode == (unsigned char) opcode;
}

Instr *BasicBlockSeq::AddInstr(BasicBlock *dest, vm::OpCode opcode, int arg, unsigned int lineno) {
    if (this->current == nullptr) {
        assert(this->begin == nullptr);

        if (this->BlockNewAppend() == nullptr)
            return nullptr;
    }

    auto *instr = this->current->AddInstr(opcode, arg, lineno);
    if (instr != nullptr)
        instr->jmp = dest;

    return instr;
}

void BasicBlockSeq::Append(BasicBlock *block) {
    if (this->begin == nullptr) {
        this->begin = block;
        this->current = block;
    } else {
        this->current->next = block;
        this->current = block;
    }
}