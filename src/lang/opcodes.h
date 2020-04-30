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
        ADD,
        CALL,
        CMP,
        DIV,
        IDIV,
        INV,
        IPADD,
        IPDIV,
        IPMUL,
        IPSUB,
        JF,     // JUMP_FALSE
        JFOP,   // JUMP_FALSE_OR_POP
        JMP,    // JUMP
        JT,     // JUMP_TRUE
        JTOP,   // JUMP_TRUE_OR_POP
        LAND,
        LDENC,
        LDGBL,
        LDLC,
        LOR,
        LSTATIC,
        LXOR,
        MK_BOUNDS,
        MK_CLOSURE,
        MK_LIST,
        MK_MAP,
        MK_SET,
        MK_TUPLE,
        MOD,
        MUL,
        NEG,
        NGV,    // NEW_GLOBAL_VARIABLE
        NLV,    // NEW_LOCAL_VARIABLE
        NOT,
        POP,
        POS,
        PRED,
        PREI,
        PSTD,
        PSTI,
        RET,
        SHL,
        SHR,
        STGBL,
        STLC,
        SUB,
        SUBSCR,
        TEST
    };
} // namespace lang

#endif // !ARGON_LANG_OPCODES_H_
