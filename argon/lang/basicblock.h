// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_BASICBLOCK_H_
#define ARGON_VM_BASICBLOCK_H_

#include <argon/vm/datatype/arstring.h>

#include <argon/vm/opcode.h>

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

    enum class JBlockType {
        LABEL,
        LOOP,
        TRAP,
        SAFE,
        SYNC,
        SWITCH
    };

    struct JBlock {
        JBlock *prev;

        argon::vm::datatype::String *label;

        BasicBlock *start;
        BasicBlock *end;

        JBlockType type;

        unsigned short nested;

        unsigned short pops;
    };

    BasicBlock *BasicBlockNew();

    BasicBlock *BasicBlockDel(BasicBlock *block);

    JBlock *JBlockNew(JBlock *prev, argon::vm::datatype::String *label, JBlockType type, unsigned short nested);

    JBlock *JBlockDel(JBlock *jb);
}

#endif // !ARGON_VM_BASICBLOCK_H_
