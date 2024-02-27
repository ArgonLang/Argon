// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_BASICBLOCK_H_
#define ARGON_LANG_COMPILER2_BASICBLOCK_H_

#include <argon/vm/opcode.h>

namespace argon::lang::compiler2 {
    struct Instr {
        Instr *next;

        struct BasicBlock *jmp;

        unsigned char opcode;
        unsigned int oparg;

        unsigned int lineno;
    };

    struct BasicBlock {
        BasicBlock *next;

        struct {
            Instr *head;
            Instr *tail;
        } instr;

        unsigned int offset;
        unsigned int size;

        Instr *AddInstr(vm::OpCode opcode, int arg, unsigned int lineno);
    };

    struct BasicBlockSeq {
        BasicBlock *begin;
        BasicBlock *current;

        BasicBlock *BlockNewAppend();

        bool CheckLastInstr(vm::OpCode opcode);

        Instr *AddInstr(BasicBlock *dest, vm::OpCode opcode, int arg, unsigned int lineno);

        void Append(BasicBlock *block);
    };

    BasicBlock *BasicBlockNew();

    BasicBlock *BasicBlockDel(BasicBlock *block);
}

#endif // !ARGON_LANG_COMPILER2_BASICBLOCK_H_
