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
        DIV,
        EQST,
        IDIV,
        LAND,
        LOR,
        LXOR,
        MOD,
        MUL,
        POP,
        SHL,
        SHR,
        SUB
    };

    constexpr short StackChange[] = {
            -1,
            -1,
            -1,
            -1,
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
            2,
            1,
            2,
            1,
            1,
            1,
            1,
            1,
            1,
            1,
            1,
            1,
            1
    };

} // namespace argon::vm

#endif // !ARGON_VM_OPCODE_H_
