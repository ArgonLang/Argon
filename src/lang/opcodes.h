// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_OPCODES_H_
#define ARGON_LANG_OPCODES_H_

namespace lang {

    using Instr32 = unsigned int;
    using Instr16 = unsigned short;
    using Instr8 = unsigned char;

    inline unsigned char I16Arg(const unsigned char *instr) { return *((Instr16 *) instr) >> (unsigned char) 8; }

    inline unsigned int I32Arg(const unsigned char *instr) { return *((Instr32 *) instr) >> (unsigned char) 8; }

    enum class OpCodes : unsigned char {
        NGV,    // NEW_GLOBAL_VARIABLE
        NLV,    // NEW_LOCAL_VARIABLE
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
        JTAP,   // JUMP_TRUE_AND_POP
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
        IPADD,
        SUB,
        IPSUB,
        MUL,
        IPMUL,
        DIV,
        IPDIV,
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
