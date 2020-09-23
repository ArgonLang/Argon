// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_OPCODES_H_
#define ARGON_LANG_OPCODES_H_

#include "utils/enum_bitmask.h"

namespace argon::lang {

    using Instr32 = unsigned int;
    using Instr16 = unsigned short;
    using Instr8 = unsigned char;

    inline unsigned char I16Arg(const unsigned char *instr) { return *((Instr16 *) instr) >> (unsigned char) 8; }

    inline unsigned int I32Arg(const unsigned char *instr) { return *((Instr32 *) instr) >> (unsigned char) 8; }

    inline unsigned char I32ExtractFlag(const unsigned char *instr) { return I32Arg(instr) >> (unsigned char) 16; }

    /*
     *  OpCode size
     *
     *  NGV:        4Bytes
     *  NLV:        2Bytes
     *  LDLC:       2Bytes
     *  STLC:       2Bytes
     *  LDENC:      2Bytes
     *  STENC:      2Bytes
     *  MK_STRUCT   2Bytes
     *  MK_TRAIT    2Bytes
     *  LDGBL:      4Bytes
     *  LDATTR:     4Bytes
     *  LSTATIC:    4Bytes
     *  LDSCOPE:    4Bytes
     *  STSCOPE:    4Bytes
     *  IMPMOD:     4Bytes
     *  IMPFRM:     4Bytes
     */

    enum class OpCodes : unsigned char {
        ADD,
        CALL,
        CMP,
        DFR,
        DIV,
        DUP,    // Duplicate elements on stack
        IDIV,
        IMPFRM,
        IMPMOD,
        INIT,   // See OpCodeINITFLAGS
        INV,
        IPADD,
        IPDIV,
        IPMUL,
        IPSUB,
        JF,     // JUMP_FALSE
        JFOP,   // JUMP_FALSE_OR_POP
        JMP,    // JUMP
        JNIL,   // JUMP IF NIL
        JT,     // JUMP_TRUE
        JTOP,   // JUMP_TRUE_OR_POP
        LAND,
        LDATTR,
        LDENC,
        LDGBL,
        LDLC,
        LDSCOPE,
        LOR,
        LSTATIC,
        LXOR,
        MK_BOUNDS,
        MK_FUNC,
        MK_LIST,
        MK_MAP,
        MK_SET,
        MK_STRUCT,
        MK_TRAIT,
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
        PB_HEAD,    // Push back head item of n position
        RET,
        SHL,
        SHR,
        SPWN,
        STATTR,
        STENC,
        STGBL,
        STLC,
        STSCOPE,
        STSUBSCR,
        SUB,
        SUBSCR,
        TEST
    };

    enum class OpCodeINITFlags : unsigned char {
        LIST = 0,
        DICT = 1
    };

    enum class MkFuncFlags : unsigned char {
        PLAIN = 0,
        VARIADIC = 1,
        CLOSURE = (unsigned char) 1 << (unsigned char) 1
    };

} // namespace lang

ENUMBITMASK_ENABLE(argon::lang::MkFuncFlags);

#endif // !ARGON_LANG_OPCODES_H_
