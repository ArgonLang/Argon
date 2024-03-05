// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/compiler2/optimizer.h>

using namespace argon::lang::compiler2;

void CodeOptimizer::OptimizeJMP(BasicBlock *begin) {
    for (auto *block = begin; block != nullptr; block = block->next) {
        for (auto *instr = block->instr.head; instr != nullptr; instr = instr->next) {
            auto *jb = instr->jmp;
            auto opcode = (vm::OpCode) instr->opcode;

            // Unoptimizable jump instructions: JFOP, JNIL, JNN, JTOP.

            if (opcode != vm::OpCode::JEX && opcode != vm::OpCode::JF
                && opcode != vm::OpCode::JMP && opcode != vm::OpCode::JT)
                continue;

            while (jb != nullptr) {
                if (jb->size == 0) {
                    jb = jb->next;
                    continue;
                }

                if (jb->instr.head->opcode != (char) vm::OpCode::JMP)
                    break;

                jb = jb->instr.head->jmp;
            }

            instr->jmp = jb;
        }
    }
}

bool CodeOptimizer::optimize(BasicBlockSeq &seq) {
    switch (this->level_) {
        case OptimizationLevel::HARD:
        case OptimizationLevel::MEDIUM:
        case OptimizationLevel::SOFT:
            this->OptimizeJMP(seq.begin);
            break;
        default:
            break;
    }

    return true;
}