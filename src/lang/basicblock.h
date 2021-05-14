// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_BASICBLOCK_H_
#define ARGON_LANG_BASICBLOCK_H_

#include <cstddef>

#include <lang/opcodes.h>

namespace argon::lang {
    class BasicBlock {
        size_t allocated_ = 0;

        void CheckSize(size_t size);

    public:
        BasicBlock *link_next = nullptr;
        BasicBlock *block_next = nullptr;

        struct {
            BasicBlock *next = nullptr;
            BasicBlock *jump = nullptr;
        } flow;

        unsigned char *instr = nullptr;
        unsigned int instr_sz = 0;

        unsigned int instr_sz_start = 0;

        bool visited = false;

        BasicBlock();

        ~BasicBlock();

        void AddInstr(Instr8 instr8);

        void AddInstr(Instr16 instr16);

        void AddInstr(Instr32 instr32);
    };
}

#endif // !ARGON_LANG_BASICBLOCK_H_
