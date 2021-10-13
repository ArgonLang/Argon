// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_BASICBLOCK_H_
#define ARGON_LANG_COMPILER_BASICBLOCK_H_

#include <object/datatype/string.h>

#include <lang/opcodes.h>

namespace argon::lang::compiler {
    struct Instr {
        unsigned char opcode;
        unsigned int oparg;

        struct BasicBlock *jmp;
        Instr *next;
        // lineno, colno
    };

    struct BasicBlock {
        BasicBlock *next;

        struct {
            Instr *head;
            Instr *tail;
        } instr;

        unsigned int i_size;
        unsigned int i_offset;

        bool seen;
    };

    struct JBlock {
        JBlock *prev;

        argon::object::String *label;

        BasicBlock *start;
        BasicBlock *end;

        unsigned short nested;
        bool loop;
    };

    BasicBlock *BasicBlockNew();

    BasicBlock *BasicBlockDel(BasicBlock *block);

    Instr *BasicBlockAddInstr(BasicBlock *block, lang::OpCodes op, int arg);

    JBlock *JBlockNew(JBlock *prev, argon::object::String *label, unsigned short nested);

    JBlock *JBlockDel(JBlock *jb);
}

#endif // !ARGON_LANG_COMPILER_BASICBLOCK_H_
