// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>

#include <object/datatype/string.h>
#include <object/datatype/error.h>

#include "basicblock.h"

using namespace argon::lang::compiler;
using namespace argon::object;

BasicBlock *argon::lang::compiler::BasicBlockNew() {
    auto *bb = (BasicBlock *) argon::memory::Alloc(sizeof(BasicBlock));

    if (bb == nullptr) {
        argon::vm::Panic(argon::object::error_out_of_memory);
        return nullptr;
    }

    argon::memory::MemoryZero(bb, sizeof(BasicBlock));
    return bb;
}

BasicBlock *argon::lang::compiler::BasicBlockDel(BasicBlock *block) {
    BasicBlock *prev;
    Instr *tmp;

    if (block == nullptr)
        return nullptr;

    prev = block->next;

    // Cleanup instr
    for (Instr *cur = block->instr.head; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        argon::memory::Free(cur);
    }

    argon::memory::Free(block);
    return prev;
}

Instr *argon::lang::compiler::BasicBlockAddInstr(BasicBlock *block, OpCodes op, int arg) {
    Instr *instr;
    unsigned short op_size;

    switch (op) {
        case OpCodes::LSTATIC:
        case OpCodes::JF:
        case OpCodes::JMP:
        case OpCodes::JTOP:
        case OpCodes::JNIL:
        case OpCodes::NJE:
        case OpCodes::NGV:
        case OpCodes::LDGBL:
        case OpCodes::CALL:
        case OpCodes::DFR:
        case OpCodes::SPWN:
        case OpCodes::MK_LIST:
        case OpCodes::MK_TUPLE:
        case OpCodes::MK_SET:
        case OpCodes::MK_MAP:
        case OpCodes::INIT:
        case OpCodes::IMPMOD:
        case OpCodes::IMPFRM:
        case OpCodes::LDSCOPE:
        case OpCodes::STSCOPE:
        case OpCodes::LDATTR:
        case OpCodes::STATTR:
        case OpCodes::UNPACK:
            op_size = 4;
            break;
        case OpCodes::STLC:
        case OpCodes::LDLC:
        case OpCodes::LDENC:
        case OpCodes::STENC:
        case OpCodes::MK_STRUCT:
        case OpCodes::MK_TRAIT:
        case OpCodes::MK_BOUNDS:
        case OpCodes::CMP:
        case OpCodes::DUP:
        case OpCodes::PB_HEAD:
            op_size = 2;
            break;
        default:
            op_size = 1;
    }

    block->i_size = op_size;

    if ((instr = (Instr *) argon::memory::Alloc(sizeof(Instr))) != nullptr) {
        argon::memory::MemoryZero(instr, sizeof(Instr));

        if (block->instr.head == nullptr) {
            block->instr.head = instr;
            block->instr.tail = instr;
        } else {
            block->instr.tail->next = instr;
            block->instr.tail = instr;
        }

        instr->opcode = (unsigned char) op;
        instr->oparg = op_size << 3 | arg; // | instr_len | argument |
    } else
        argon::vm::Panic(argon::object::error_out_of_memory);

    return instr;
}

JBlock *argon::lang::compiler::JBlockNew(JBlock *prev, String *label, unsigned short nested) {
    auto *j = (JBlock *) argon::memory::Alloc(sizeof(JBlock));

    if (j == nullptr) {
        argon::vm::Panic(argon::object::error_out_of_memory);
        return nullptr;
    }

    j->prev = prev;
    j->label = IncRef(label);
    j->start = nullptr;
    j->end = nullptr;
    j->nested = nested;
    j->loop = false;

    return j;
}

JBlock *argon::lang::compiler::JBlockDel(JBlock *jb) {
    JBlock *prev;

    if (jb == nullptr)
        return nullptr;

    prev = jb->prev;

    Release(jb->label);

    argon::memory::Free(jb);

    return prev;
}