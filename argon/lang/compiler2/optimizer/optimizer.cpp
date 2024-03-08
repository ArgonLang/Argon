// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>

#include <argon/vm/memory/memory.h>

#include <argon/lang/exception.h>
#include <argon/lang/compiler2/optimizer/optimizer.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;

bool CodeOptimizer::SimplifyConstOP(Instr *left, Instr *right, Instr *op, bool &must_update) {
    ARC res;

    if ((vm::OpCode) left->opcode != vm::OpCode::LSTATIC)
        return false;

    if ((vm::OpCode) right->opcode != vm::OpCode::LSTATIC)
        return false;

    int op_off;
    switch ((vm::OpCode) op->opcode) {
        case vm::OpCode::ADD:
            op_off = offsetof(vm::datatype::OpSlots, add);
            break;
        case vm::OpCode::DIV:
            op_off = offsetof(vm::datatype::OpSlots, div);
            break;
        case vm::OpCode::IDIV:
            op_off = offsetof(vm::datatype::OpSlots, idiv);
            break;
        case vm::OpCode::LAND:
            op_off = offsetof(vm::datatype::OpSlots, l_and);
            break;
        case vm::OpCode::LOR:
            op_off = offsetof(vm::datatype::OpSlots, l_or);
            break;
        case vm::OpCode::LXOR:
            op_off = offsetof(vm::datatype::OpSlots, l_xor);
            break;
        case vm::OpCode::MOD:
            op_off = offsetof(vm::datatype::OpSlots, mod);
            break;
        case vm::OpCode::MUL:
            op_off = offsetof(vm::datatype::OpSlots, mul);
            break;
        case vm::OpCode::SHL:
            op_off = offsetof(vm::datatype::OpSlots, shl);
            break;
        case vm::OpCode::SHR:
            op_off = offsetof(vm::datatype::OpSlots, shr);
            break;
        case vm::OpCode::SUB:
            op_off = offsetof(vm::datatype::OpSlots, sub);
            break;
        default:
            return false;
    }

    auto *statics = this->unit_->statics;
    auto larg = left->oparg;
    auto rarg = right->oparg;

    res = vm::datatype::ExecBinaryOp(statics->objects[larg], statics->objects[rarg], op_off);
    if (!res) {
        if (vm::IsPanicking())
            throw DatatypeException();

        return false;
    }

    auto index = this->LookupInsertConstant(res.Get());

    left->oparg = index;
    left->next = op->next;

    if (--this->unit_->statics_usg_count[larg] == 0) {
        Release(statics->objects + larg);
        must_update = true;
    }

    if (--this->unit_->statics_usg_count[rarg] == 0) {
        Release(statics->objects + rarg);
        must_update = true;
    }

    vm::memory::Free(right);
    vm::memory::Free(op);

    return true;
}

int CodeOptimizer::LookupInsertConstant(ArObject *constant) {
    int index;

    auto *res = DictLookup(this->unit_->statics_map, constant);
    if (res != nullptr) {
        index = (int) ((vm::datatype::Integer *) res)->sint;

        Release(res);

        return index;
    }

    index = (int) this->unit_->statics->length;

    auto *value = IntNew(index);
    if (value == nullptr)
        throw DatatypeException();

    if (!ListAppend(this->unit_->statics, constant)) {
        Release(value);

        throw DatatypeException();
    }

    if (!DictInsert(this->unit_->statics_map, constant, (ArObject *) value)) {
        Release(value);

        throw DatatypeException();
    }

    Release(value);

    this->unit_->IncStaticUsage(index);

    return index;
}

void CodeOptimizer::OptimizeConstOP() {
    bool must_update = false;

    for (auto *block = this->unit_->bbb.begin; block != nullptr; block = block->next) {
        Instr *li = block->instr.head;
        Instr *ri;
        Instr *op;

        while (li != nullptr) {
            ri = li->next;
            op = ri != nullptr ? ri->next : nullptr;

            if (op == nullptr)
                break;

            auto code = (vm::OpCode) op->opcode;
            if (this->SimplifyConstOP(li, ri, op, must_update)) {
                block->size -= vm::OpCodeOffset[(int) vm::OpCode::LSTATIC]
                               + vm::OpCodeOffset[(int) code];
                continue;
            }

            li = ri;
        }
    }

    if (!must_update)
        return;

    auto remap = (int *) vm::memory::Calloc(this->unit_->statics->length * sizeof(int));

    int index = 0;
    for (auto i = 0; i < this->unit_->statics->length; i++) {
        if (this->unit_->statics->objects[i] != nullptr) {
            this->unit_->statics->objects[index] = this->unit_->statics->objects[i];
            remap[i] = index++;
        }
    }

    this->unit_->statics->length = index;

    // Updates all instructions that use the index to the static resource table as an argument
    for (auto *block = this->unit_->bbb.begin; block != nullptr; block = block->next) {
        for (auto *instr = block->instr.head; instr != nullptr; instr = instr->next) {
            if ((vm::OpCode) instr->opcode == vm::OpCode::LDATTR
                || (vm::OpCode) instr->opcode == vm::OpCode::LDMETH
                || (vm::OpCode) instr->opcode == vm::OpCode::LDSCOPE
                || (vm::OpCode) instr->opcode == vm::OpCode::LSTATIC
                || (vm::OpCode) instr->opcode == vm::OpCode::IMPFRM
                || (vm::OpCode) instr->opcode == vm::OpCode::IMPMOD)
                instr->oparg = remap[instr->oparg];
        }
    }

    vm::memory::Free(remap);
}

void CodeOptimizer::OptimizeJMP() {
    for (auto *block = this->unit_->bbb.begin; block != nullptr; block = block->next) {
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

bool CodeOptimizer::optimize() {
    switch (this->level_) {
        case OptimizationLevel::HARD:
        case OptimizationLevel::MEDIUM:
            this->OptimizeConstOP();
        case OptimizationLevel::SOFT:
            this->OptimizeJMP();
            break;
        default:
            break;
    }

    return true;
}