// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <argon/vm/memory/memory.h>

#include <argon/lang/basicblock.h>

using namespace argon::lang;

Instr *BasicBlock::AddInstr(vm::OpCode opcode, int arg) {
    auto *i = (Instr *) argon::vm::memory::Alloc(sizeof(Instr));

    if (i != nullptr) {
        i->next = nullptr;

        assert(((unsigned char) opcode) < sizeof(vm::OpCodeOffset));

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

        i->jmp = nullptr;
        i->lineno = 0;
    }

    return i;
}

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

JBlock *argon::lang::JBlockNew(JBlock *prev, argon::vm::datatype::String *label, unsigned short nested) {
    auto *j = (JBlock *) argon::vm::memory::Calloc(sizeof(JBlock));

    if (j != nullptr) {
        j->prev = prev;
        j->label = IncRef(label);
        j->nested = nested;
    }

    return j;
}

JBlock *argon::lang::JBlockDel(JBlock *jb) {
    JBlock *prev;

    if (jb == nullptr)
        return nullptr;

    prev = jb->prev;

    Release(jb->label);

    vm::memory::Free(jb);

    return prev;
}