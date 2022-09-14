// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_BASICBLOCK_H_
#define ARGON_VM_BASICBLOCK_H_

#include <vm/opcode.h>

namespace argon::lang {
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

        unsigned int size;
        unsigned int offset;

        bool seen;

        Instr *AddInstr(vm::OpCode opcode, int arg);
    };

    BasicBlock *BasicBlockNew();

    BasicBlock *BasicBlockDel(BasicBlock *block);
}

#endif // !ARGON_VM_BASICBLOCK_H_
