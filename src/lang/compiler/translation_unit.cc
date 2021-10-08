// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>
#include <object/datatype/error.h>

#include "translation_unit.h"

using namespace argon::lang::compiler;
using namespace argon::object;

bool argon::lang::compiler::TranslationUnitIsFreeVar(TranslationUnit *unit, String *name) {
    // Look back in the TranslationUnits,
    // if a variable with the same name exists and is declared or free
    // in turn then this is a free variable
    Symbol *sym;

    for (TranslationUnit *tu = unit; tu != nullptr && tu->scope == TUScope::FUNCTION; tu = tu->prev) {
        sym = SymbolTableLookup(unit->symt, name);
        if (sym != nullptr) {
            if ((sym->declared || sym->free)) {
                Release(sym);
                return true;
            }
            Release(sym);
        }
    }

    return false;
}

TranslationUnit *argon::lang::compiler::TranslationUnitNew(String *name, TUScope scope, SymbolTable *symt) {
    auto *tu = (TranslationUnit *) argon::memory::Alloc(sizeof(TranslationUnit));

    if (tu != nullptr) {
        argon::memory::MemoryZero(tu, sizeof(TranslationUnit));

        if ((tu->symt = symt) == nullptr)
            goto ERROR;

        if ((tu->statics_map = MapNew()) == nullptr)
            goto ERROR;

        if ((tu->statics = ListNew()) == nullptr)
            goto ERROR;

        if ((tu->names = ListNew()) == nullptr)
            goto ERROR;

        if ((tu->locals = ListNew()) == nullptr)
            goto ERROR;

        if ((tu->enclosed = ListNew()) == nullptr)
            goto ERROR;

        tu->name = IncRef(name);
        tu->scope = scope;
    } else
        argon::vm::Panic(argon::object::error_out_of_memory);

    return tu;

    ERROR:
    Release(tu->statics_map);
    Release(tu->statics);
    Release(tu->names);
    Release(tu->locals);
    Release(tu->enclosed);
    argon::memory::Free(tu);
    return nullptr;
}

TranslationUnit *argon::lang::compiler::TranslationUnitDel(TranslationUnit *unit) {
    TranslationUnit *prev = unit->prev;
    BasicBlock *tmp = unit->bb.start;

    // Free all BasicBlock
    while ((tmp = BasicBlockDel(tmp)) != nullptr);

    Release(unit->name);
    Release(unit->statics_map);
    Release(unit->statics);
    Release(unit->names);
    Release(unit->locals);
    Release(unit->enclosed);
    argon::memory::Free(unit);

    return prev;
}

BasicBlock *argon::lang::compiler::TranslationUnitBlockNew(TranslationUnit *unit) {
    auto *bb = BasicBlockNew();

    if (bb != nullptr)
        TranslationUnitBlockAppend(unit, bb);

    return bb;
}

void argon::lang::compiler::TranslationUnitBlockAppend(TranslationUnit *unit, BasicBlock *block) {
    if (unit->bb.start == nullptr) {
        unit->bb.start = block;
        unit->bb.cur = block;
    } else {
        unit->bb.cur->next = block;
        unit->bb.cur = block;
    }
}

void argon::lang::compiler::TranslationUnitDecStack(TranslationUnit *unit, unsigned short size) {
    unit->stack.current -= size;
    assert(unit->stack.current < 0x00FFFFFF);
}

void argon::lang::compiler::TranslationUnitIncStack(TranslationUnit *unit, unsigned short size) {
    unit->stack.current += size;
    if (unit->stack.current > unit->stack.required)
        unit->stack.required = unit->stack.current;
}

