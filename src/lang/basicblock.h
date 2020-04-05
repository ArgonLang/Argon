// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_BASICBLOCK_H_
#define ARGON_LANG_BASICBLOCK_H_

#define ARGON_LANG_BASICBLOCK_STARTSZ   16
#define ARGON_LANG_BASICBLOCK_INCSZ     8

#include "opcodes.h"

namespace lang {
    class BasicBlock {
        size_t allocated_ = 0;

        void CheckSize(size_t size);

    public:
        BasicBlock *link_next = nullptr;
        BasicBlock *flow_next = nullptr;
        BasicBlock *flow_else = nullptr;

        unsigned char *instrs = nullptr;

        unsigned int start = 0;
        unsigned int instr_sz = 0;

        bool visited = false;

        BasicBlock();

        ~BasicBlock();

        void AddInstr(Instr8 instr);

        void AddInstr(Instr16 instr);

        void AddInstr(Instr32 instr);
    };
} // namespace lang

#endif // !ARGON_LANG_BASICBLOCK_H_
