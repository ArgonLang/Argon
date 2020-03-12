// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_OPCODES_H_
#define ARGON_LANG_OPCODES_H_

namespace lang {
    enum class OpCodes : unsigned char {
        PNOB, // POP_NIL_OR_BACK
        LOR,
        LXOR,
        LAND,
        CMP,
        SHL,
        SHR,
        ADD,
        SUB,
        MUL,
        DIV,
        IDIV,
        MOD,
        UNARY_NOT,
        UNARY_INV,
        UNARY_POS,
        UNARY_NEG,
        PREFX_INC,
        PREFX_DEC
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
