// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "basicblock.h"

using namespace lang;

lang::BasicBlock::BasicBlock() {
    this->instrs.reserve(ARGON_LANG_BASICBLOCK_STARTSZ);
}

void lang::BasicBlock::AddInstr(InstrSz instr) {
    this->instrs.push_back(instr);
}

BasicBlock::~BasicBlock() {
    delete this->link_next;
}
