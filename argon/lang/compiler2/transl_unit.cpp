// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/exception.h>

#include <argon/lang/compiler2/transl_unit.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;

bool MakeQName(const TranslationUnit *prev, TranslationUnit *unit, String *name) {
    const char *sep = "%s.%s";

    if (prev != nullptr && prev->qname != nullptr && name != nullptr && !StringIsEmpty(name)) {
        if (prev->symt->type != SymbolType::MODULE)
            sep = "%s::%s";

        if ((unit->qname = StringFormat(sep, ARGON_RAW_STRING(prev->qname), ARGON_RAW_STRING(name))) == nullptr)
            return false;
    } else
        unit->qname = IncRef(name);

    unit->name = IncRef(name);
    return true;
}

BasicBlock *TranslationUnit::BlockAppend(BasicBlock *block) {
    this->bbb.Append(block);
    return block;
}

BasicBlock *TranslationUnit::BlockNew() {
    auto *block = this->bbb.BlockNewAppend();
    if (block == nullptr)
        throw DatatypeException();

    return block;
}

JBlock *TranslationUnit::JBFindLabel(const String *label, unsigned short &out_pops) const {
    out_pops = 0;

    for (auto *block = this->jblock; block != nullptr; block = block->prev) {
        out_pops += block->pops;

        if (block->type != JBlockType::LOOP)
            continue;

        if (block->label != nullptr && StringCompare(block->label, label) == 0)
            return block;
    }

    return nullptr;
}

JBlock *TranslationUnit::JBPush(String *label, BasicBlock *begin, BasicBlock *end, JBlockType type) {
    auto *j = JBlockNew(this->jblock, label, type);
    if (j == nullptr)
        throw DatatypeException();

    j->begin = begin;
    j->end = end;

    this->jblock = j;

    return j;
}

JBlock *TranslationUnit::JBPush(String *label, JBlockType type) {
    auto *j = JBlockNew(this->jblock, label, type);
    if (j == nullptr)
        throw DatatypeException();

    auto *begin = this->bbb.current;

    if (begin == nullptr || begin->size > 0) {
        begin = BasicBlockNew();
        if (begin == nullptr) {
            JBlockDel(j);

            throw DatatypeException();
        }

        this->bbb.Append(begin);
    }

    j->begin = begin;

    this->jblock = j;

    return j;
}

JBlock *TranslationUnit::JBPush(BasicBlock *begin, BasicBlock *end) {
    String *label = nullptr;

    if (this->jblock != nullptr && this->jblock->type == JBlockType::LABEL)
        label = this->jblock->label;

    return this->JBPush(label, begin, end, JBlockType::LOOP);
}

JBlock *TranslationUnit::JBPush(BasicBlock *begin, BasicBlock *end, unsigned short pops) {
    auto jb = this->JBPush(begin, end);

    jb->pops = pops;

    return jb;
}

bool TranslationUnit::CheckBlock(JBlockType expected) const {
    return this->jblock != nullptr && this->jblock->type == expected;
}

bool TranslationUnit::IsFreeVar(const String *id) const {
    // Look back in the TranslationUnits,
    // if a variable with the same name exists and is declared or free
    // in turn then this is a free variable
    SymbolT *sym;

    for (TranslationUnit *tu = this->prev; tu != nullptr; tu = tu->prev) {
        if (tu->symt->type == SymbolType::STRUCT || tu->symt->type == SymbolType::TRAIT)
            continue;

        if ((sym = tu->symt->SymbolLookup(id)) != nullptr) {
            // WARNING: sym->nested must be greater than 0,
            // otherwise this is a global variable.
            if (sym->type == SymbolType::VARIABLE
                && sym->nested > 0
                && (sym->declared || sym->free)) {
                Release(sym);

                return true;
            }

            Release(sym);
        }
    }

    return false;
}

void TranslationUnit::JBPop() {
    this->jblock = JBlockDel(this->jblock);
}

void TranslationUnit::Emit(vm::OpCode op, int arg, BasicBlock *dest, const scanner::Loc *loc) {
    unsigned int lineno = 0;

    if (loc != nullptr)
        lineno = (unsigned int) loc->start.line;

    if (this->bbb.AddInstr(dest, op, arg, lineno) == nullptr)
        throw DatatypeException();

    switch (op) {
        case vm::OpCode::CALL:
        case vm::OpCode::DFR:
        case vm::OpCode::INIT:
        case vm::OpCode::SPW:
            this->DecrementStack(arg & 0xFFFF);
            break;
        case vm::OpCode::DUP:
            this->IncrementStack(arg);
            break;
        case vm::OpCode::MKDT:
        case vm::OpCode::MKLT:
        case vm::OpCode::MKST:
        case vm::OpCode::MKTP:
        case vm::OpCode::POPGT:
            this->DecrementStack(arg);
            break;
        default:
            break;
    }

    this->IncrementStack(vm::StackChange[(unsigned char) op]);
}

TranslationUnit *argon::lang::compiler2::TranslationUnitNew(TranslationUnit *prev, String *name, SymbolType type) {
    auto *symt = SymbolTableNew(prev != nullptr ? prev->symt : nullptr, name, type);
    if (symt == nullptr)
        return nullptr;

    auto *tu = (TranslationUnit *) argon::vm::memory::Calloc(sizeof(TranslationUnit));

    if (tu != nullptr) {
        tu->prev = prev;

        tu->symt = symt;

        if (!MakeQName(prev, tu, name)) {
            argon::vm::memory::Free(tu);
            return nullptr;
        }

        if (symt->type != SymbolType::STRUCT && symt->type != SymbolType::TRAIT) {
            if ((tu->statics_map = DictNew()) == nullptr)
                goto ERROR;

            if ((tu->statics = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->names = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->locals = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->enclosed = ListNew()) == nullptr)
                goto ERROR;
        }
    }

    return tu;

    ERROR:
    Release(symt);

    Release(tu->name);
    Release(tu->qname);
    Release(tu->statics_map);
    Release(tu->statics);
    Release(tu->names);
    Release(tu->locals);

    argon::vm::memory::Free(tu);

    return nullptr;
}

TranslationUnit *argon::lang::compiler2::TranslationUnitDel(TranslationUnit *unit) {
    auto *prev = unit->prev;

    Release(unit->symt);
    Release(unit->name);
    Release(unit->qname);
    Release(unit->statics_map);
    Release(unit->statics);
    Release(unit->names);
    Release(unit->locals);
    Release(unit->enclosed);

    argon::vm::memory::Free(unit);

    return prev;
}
