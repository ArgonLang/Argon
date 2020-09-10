// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "basicblock.h"

using namespace argon::memory;
using namespace argon::lang;

#define STARTSZ  16
#define INCSZ    8

BasicBlock::BasicBlock() {
    if ((this->instr = (unsigned char *) Alloc(STARTSZ)) == nullptr)
        throw std::bad_alloc();
    this->allocated_ = STARTSZ;
}

BasicBlock::~BasicBlock() {
    Free(this->instr);
}

void BasicBlock::AddInstr(Instr8 instr8) {
    this->CheckSize(sizeof(Instr8));
    *(this->instr + this->instr_sz) = (Instr8) instr8;
    this->instr_sz++;
}

void BasicBlock::AddInstr(Instr16 instr16) {
    this->CheckSize(sizeof(Instr16));
    *((Instr16 *) (this->instr + this->instr_sz)) = (Instr16) instr16;
    this->instr_sz += sizeof(Instr16);
}

void BasicBlock::AddInstr(Instr32 instr32) {
    this->CheckSize(sizeof(Instr32));
    *((Instr32 *) (this->instr + this->instr_sz)) = instr32;
    this->instr_sz += sizeof(Instr32);
}

void BasicBlock::CheckSize(size_t size) {
    if (this->instr_sz + size >= this->allocated_) {
        auto tmp = (unsigned char *) argon::memory::Realloc(this->instr, this->allocated_ + INCSZ);

        if (tmp == nullptr)
            throw std::bad_alloc();

        this->instr = tmp;
        this->allocated_ += INCSZ;
    }
}
