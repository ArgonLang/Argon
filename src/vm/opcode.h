// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_OPCODE_H_
#define ARGON_VM_OPCODE_H_

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
        CMP,
        DEC,
        DIV,
        DUP,
        EQST,
        IDIV,
        INC,
        INIT,
        INV,
        JF,
        JFOP,
        JMP,
        JTOP,
        LAND,
        LDLC,
        LDENC,
        LDGBL,
        LOR,
        LSTATIC,
        LXOR,
        MOD,
        MKLT,
        MKTP,
        MKDT,
        MKST,
        MUL,
        NEG,
        NOT,
        POP,
        POS,
        SHL,
        SHR,
        STENC,
        STLC,
        STGBL,
        SUB
    };

    constexpr short StackChange[] = {
            -1,
            -1,
            0,
            -1,
            1,
            -1,
            -1,
            0,
            1,
            -1,
            -1,
            -1,
            0,
            -1,
            -1,
            1,
            1,
            1,
            -1,
            1,
            -1,
            -1,
            1,
            1,
            1,
            1,
            -1,
            0,
            0,
            -1,
            0,
            -1,
            -1,
            -1,
            -1,
            -1,
            -1
    };

    constexpr short OpCodeOffset[] = {
            1,
            2,
            1,
            1,
            1,
            2,
            1,
            1,
            4,
            1,
            4,
            4,
            4,
            4,
            1,
            2,
            2,
            4,
            1,
            4,
            1,
            1,
            4,
            4,
            4,
            4,
            1,
            1,
            1,
            1,
            1,
            1,
            1,
            2,
            2,
            4,
            1
    };

    enum class OpCodeInitMode : unsigned char {
        POSITIONAL,
        KWARGS
    };

} // namespace argon::vm

#endif // !ARGON_VM_OPCODE_H_
