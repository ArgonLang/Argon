// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_OPCODES_H_
#define ARGON_LANG_OPCODES_H_

namespace lang {

    using InstrSz = unsigned int;

    enum class OpCodes : unsigned char {
        STGBL,
        STLC,
        LDGBL,
        LDLC,
        MK_TUPLE,
        MK_LIST,
        MK_SET,
        MK_MAP,
        JMP,    // JUMP
        JF,     // JUMP_FALSE
        JT,     // JUMP_TRUE
        JTOP,   // JUMP_TRUE_OR_POP
        JFOP,   // JUMP_FALSE_OR_POP
        LOR,
        LXOR,
        LAND,
        TEST,
        CMP,
        SHL,
        SHR,
        ADD,
        SUB,
        MUL,
        DIV,
        IDIV,
        MOD,
        NOT,
        INV,
        POS,
        NEG,
        PREI,
        PRED,
        PSTI,
        PSTD,
        LSTATIC
    };

    enum class CompareMode : unsigned char {
        EQ,
        NE,
        GE,
        GEQ,
        LE,
        LEQ
    };
} // namespace lang

#endif // !ARGON_LANG_OPCODES_H_
