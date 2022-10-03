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

    inline unsigned char I32Flag(const unsigned char *instr) {
        return (unsigned char) (I32Arg(instr) >> (unsigned char) 16);
    }

    enum class OpCode : unsigned char {
        ADD,
        AWAIT,
        CALL,
        CMP,
        DEC,
        DFR,
        DIV,
        DUP,
        EQST,
        EXTD,
        IDIV,
        INC,
        INIT,
        INV,
        JF,
        JFOP,
        JMP,
        JNIL,
        JTOP,
        LAND,
        LDATTR,
        LDENC,
        LDGBL,
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
        MKST,
        MKTP,
        MOD,
        MUL,
        NEG,
        NGV,
        NOT,
        PANIC,
        PLT,
        POP,
        POS,
        RET,
        SHL,
        SHR,
        SPW,
        STENC,
        STGBL,
        STLC,
        SUB,
        SUBSCR,
        YLD
    };

    constexpr short StackChange[] = {
            -1,
            0,
            0,
            -1,
            0,
            -1,
            -1,
            0,
            -1,
            -1,
            -1,
            0,
            0,
            0,
            -1,
            -1,
            0,
            0,
            -1,
            -1,
            1,
            1,
            1,
            1,
            0,
            1,
            -1,
            1,
            -1,
            -1,
            0,
            1,
            1,
            1,
            1,
            -1,
            -1,
            -1,
            0,
            -1,
            -1,
            -1,
            -1,
            0,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1
    };

    constexpr short OpCodeOffset[] = {
            1,
            4,
            4,
            2,
            1,
            4,
            1,
            2,
            2,
            1,
            1,
            1,
            4,
            1,
            4,
            4,
            4,
            4,
            4,
            1,
            4,
            2,
            4,
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
            4,
            4,
            1,
            1,
            1,
            4,
            1,
            1,
            1,
            1,
            1,
            1,
            1,
            1,
            4,
            2,
            4,
            2,
            1,
            1,
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
