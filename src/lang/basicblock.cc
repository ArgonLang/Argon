// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <memory/memory.h>
#include "basicblock.h"

using namespace lang;

BasicBlock::BasicBlock() {
    this->instrs = (unsigned char *) argon::memory::Alloc(ARGON_LANG_BASICBLOCK_STARTSZ);
    this->allocated_ = ARGON_LANG_BASICBLOCK_STARTSZ;
}

BasicBlock::~BasicBlock() { argon::memory::Free(this->instrs); }

void BasicBlock::AddInstr(Instr8 instr) {
    this->CheckSize(sizeof(Instr8));
    *(this->instrs + this->instr_sz) = (Instr8) instr;
    this->instr_sz++;
}

void BasicBlock::AddInstr(Instr16 instr) {
    this->CheckSize(sizeof(Instr16));
    *((Instr16 *) (this->instrs + this->instr_sz)) = (Instr16) instr;
    this->instr_sz += sizeof(Instr16);
}

void BasicBlock::AddInstr(Instr32 instr) {
    this->CheckSize(sizeof(Instr32));
    *((Instr32 *) (this->instrs + this->instr_sz)) = instr;
    this->instr_sz += sizeof(Instr32);
}

void BasicBlock::CheckSize(size_t size) {
    if (this->instr_sz + size >= this->allocated_) {
        auto tmp = (unsigned char *) argon::memory::Realloc(this->instrs,
                                                            this->allocated_ + ARGON_LANG_BASICBLOCK_INCSZ);
        assert(tmp != nullptr); // TODO: raise exception ?!
        this->instrs = tmp;
        this->allocated_ += ARGON_LANG_BASICBLOCK_INCSZ;
    }
}
