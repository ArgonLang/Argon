// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_OPCODE_H_
#define ARGON_VM_OPCODE_H_

#include "util/enum_bitmask.h"

namespace argon::vm {
    using Instr32 = unsigned int;
    using Instr16 = unsigned short;
    using Instr8 = unsigned char;

    inline unsigned char I16Arg(const unsigned char *instr) { return *((const Instr16 *) instr) >> (unsigned char) 8; }

    inline unsigned int I32Arg(const unsigned char *instr) { return *((const Instr32 *) instr) >> (unsigned char) 8; }

    template<typename T>
    inline T I32Flag(const unsigned char *instr) {
        return (T) (I32Arg(instr) >> (unsigned char) 16);
    }

    enum class OpCode : unsigned char {
        ADD,
        AWAIT,
        CALL,
        CMP,
        CNT,
        DEC,
        DFR,
        DIV,
        DUP,
        EQST,
        EXTD,
        IDIV,
        IMPALL,
        IMPFRM,
        IMPMOD,
        INC,
        INIT,
        INV,
        IPADD,
        IPSUB,
        JF,
        JFOP,
        JMP,
        JNIL,
        JT,
        JTOP,
        LAND,
        LDATTR,
        LDENC,
        LDGBL,
        LDITER,
        LDLC,
        LDMETH,
        LDSCOPE,
        LOR,
        LSTATIC,
        LXOR,
        MKBND,
        MKDT,
        MKFN,
        MKLT,
        MKNS,
        MKST,
        MKSTRUCT,
        MKTP,
        MKTRAIT,
        MOD,
        MUL,
        NEG,
        NGV,
        NJE,
        NOT,
        NSTORE,
        PANIC,
        PBHEAD,
        PLT,
        POP,
        POS,
        RET,
        SHL,
        SHR,
        SPW,
        STATTR,
        STENC,
        STGBL,
        STLC,
        STSCOPE,
        STSUBSCR,
        SUB,
        SUBSCR,
        TEST,
        UNPACK,
        YLD
    };

    constexpr short StackChange[] = {
            -1,
            0,
            0,
            -1,
            -1,
            0,
            -1,
            -1,
            0,
            -1,
            -1,
            -1,
            -1,
            1,
            1,
            0,
            0,
            0,
            -1,
            -1,
            -1,
            -1,
            0,
            0,
            -1,
            -1,
            -1,
            0,
            1,
            1,
            0,
            1,
            1,
            0,
            -1,
            1,
            -1,
            -1,
            1,
            -2,
            1,
            1,
            1,
            -3,
            1,
            -3,
            -1,
            -1,
            0,
            -1,
            1,
            -1,
            -2,
            -1,
            0,
            -1,
            -1,
            0,
            -1,
            -1,
            -1,
            -1,
            -2,
            -1,
            -1,
            -1,
            -2,
            -3,
            -1,
            -1,
            0,
            -1,
            -1
    };

    constexpr short OpCodeOffset[] = {
            1,
            1,
            4,
            2,
            1,
            1,
            4,
            1,
            2,
            2,
            1,
            1,
            1,
            4,
            4,
            1,
            4,
            1,
            1,
            1,
            4,
            4,
            4,
            4,
            4,
            4,
            1,
            4,
            2,
            4,
            1,
            2,
            4,
            4,
            1,
            4,
            1,
            1,
            4,
            4,
            4,
            1,
            4,
            4,
            4,
            4,
            1,
            1,
            1,
            4,
            4,
            1,
            2,
            1,
            2,
            1,
            1,
            1,
            1,
            1,
            1,
            4,
            4,
            2,
            4,
            2,
            4,
            1,
            1,
            1,
            1,
            2,
            1
    };

    enum class OpCodeInitMode : unsigned char {
        POSITIONAL,
        KWARGS
    };

    enum class OpCodeCallMode : unsigned char {
        FASTCALL = 0,
        REST_PARAMS = 1,
        KW_PARAMS = 1 << 1u
    };
} // namespace argon::vm

ENUMBITMASK_ENABLE(argon::vm::OpCodeCallMode);

#endif // !ARGON_VM_OPCODE_H_
