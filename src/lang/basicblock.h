// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_BASICBLOCK_H_
#define ARGON_LANG_BASICBLOCK_H_

#define ARGON_LANG_BASICBLOCK_STARTSZ     16

#include <vector>

#include "opcodes.h"

namespace lang {
    class BasicBlock {
    public:
        BasicBlock *link_next = nullptr;
        BasicBlock *flow_next = nullptr;
        BasicBlock *flow_else = nullptr;

        std::vector<InstrSz> instrs;

        BasicBlock();

        ~BasicBlock();

        void AddInstr(InstrSz instr);
    };
} // namespace lang

#endif // !ARGON_LANG_BASICBLOCK_H_
