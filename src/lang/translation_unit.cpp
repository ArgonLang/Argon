// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "compilererr.h"
#include "translation_unit.h"

using namespace argon::lang;
using namespace argon::vm::datatype;

bool MakeQName(const TranslationUnit *prev, TranslationUnit *unit, String *name) {
    const char *sep = "%s.%s";

    if (prev != nullptr && name != nullptr && !StringIsEmpty(name)) {
        if (prev->symt->type != SymbolType::MODULE)
            sep = "%s::%s";

        if ((unit->qname = StringFormat(sep, ARGON_RAW_STRING(prev->qname), ARGON_RAW_STRING(name))) == nullptr)
            return false;
    } else
        unit->qname = IncRef(name);

    unit->name = IncRef(name);
    return true;
}

void BlockAppend(TranslationUnit *unit, BasicBlock *block) {
    if (unit->bb.start == nullptr) {
        unit->bb.start = block;
        unit->bb.cur = block;
    } else {
        unit->bb.cur->next = block;
        unit->bb.cur = block;
    }
}

void TranslationUnit::Emit(vm::OpCode opcode, int arg, BasicBlock *dest, const scanner::Loc *loc) {
    auto *instr = this->bb.cur->AddInstr(opcode, arg);
    if (instr != nullptr) {
        instr->jmp = dest;

        if (loc != nullptr)
            instr->lineno = (unsigned int) loc->start.line;

        assert(((unsigned char) opcode) < sizeof(vm::StackChange));

        this->IncrementStack(vm::StackChange[(unsigned char) opcode]);

        return;
    }

    throw DatatypeException();
}

TranslationUnit *argon::lang::TranslationUnitNew(TranslationUnit *prev, String *name, SymbolT *symt) {
    auto *tu = (TranslationUnit *) argon::vm::memory::Alloc(sizeof(TranslationUnit));

    if (tu != nullptr) {
        tu->prev = prev;

        tu->symt = IncRef(symt);

        if (!MakeQName(prev, tu, name)) {
            argon::vm::memory::Free(tu);
            return nullptr;
        }

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

        auto *block = BasicBlockNew();
        if (block == nullptr)
            goto ERROR;

        BlockAppend(tu, block);
    }

    return tu;

    ERROR:
    Release(tu->symt);
    Release(tu->name);
    Release(tu->qname);
    Release(tu->statics_map);
    Release(tu->statics);
    Release(tu->names);
    Release(tu->locals);

    argon::vm::memory::Free(tu);

    return nullptr;
}

TranslationUnit *argon::lang::TranslationUnitDel(TranslationUnit *unit) {
    auto *prev = unit->prev;

    // Free all BasicBlock
    BasicBlock *tmp = unit->bb.start;
    while ((tmp = BasicBlockDel(tmp)) != nullptr);

    Release(unit->symt);
    Release(unit->name);
    Release(unit->qname);
    Release(unit->statics_map);
    Release(unit->statics);
    Release(unit->names);
    Release(unit->locals);

    argon::vm::memory::Free(unit);

    return prev;
}