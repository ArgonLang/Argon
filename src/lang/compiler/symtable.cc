// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include "symtable.h"

using namespace argon::object;
using namespace argon::lang::compiler;

ArObject *symbol_compare(Symbol *self, ArObject *other, CompareMode mode) {
    auto *o = (Symbol *) other;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_TYPEOF(other, type_symbol_) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(Equal(self->name, o->name) && self->nested == o->nested && self->id == o->id &&
                        self->declared == o->declared && self->free == o->free);
}

ArObject *symbol_str(Symbol *self) {
    return StringNewFormat("symbol(name: %s, nested: %d, id: %d, declared: %s, free: %s)", self->name->buffer,
                           self->nested, self->id, self->declared, self->free);
}

ArSize symbol_hash(Symbol *self) {
    ArSize base = Hash(self->name);

    base ^= self->id + self->nested;

    return base;
}

bool symbol_is_true(ArObject *self) {
    return true;
}

void symbol_cleanup(Symbol *self) {
    Release(self->name);
    Release(self->symt);
}

const TypeInfo SymbolType_ = {
        TYPEINFO_STATIC_INIT,
        "Symbol",
        nullptr,
        sizeof(Symbol),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) symbol_cleanup,
        nullptr,
        (CompareOp) symbol_compare,
        symbol_is_true,
        (SizeTUnaryOp) symbol_hash,
        (UnaryOp) symbol_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::lang::compiler::type_symbol_ = &SymbolType_;

Symbol *argon::lang::compiler::SymbolNew() {
    auto *sym = ArObjectNew<Symbol>(RCType::INLINE, type_symbol_);

    if (sym != nullptr) {
        sym->name = nullptr;
        sym->symt = nullptr;

        sym->kind = SymbolType::UNKNOWN;

        sym->nested = 0;
        sym->id = 0;

        sym->declared = false;
        sym->free = false;
    }

    return sym;
}

// *** SYMBOL TABLE ***

ArObject *symtable_compare(SymbolTable *self, ArObject *other, CompareMode mode) {
    auto *o = (SymbolTable *) other;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_TYPEOF(other, type_symtable_) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(Equal(self->map, o->map) && Equal(self->namespaces, o->namespaces));
}

bool symtable_is_true(ArObject *self) {
    return true;
}

void symtable_cleanup(SymbolTable *self) {
    Release(self->map);
    Release(self->namespaces);
}

const TypeInfo SymbolTableType = {
        TYPEINFO_STATIC_INIT,
        "SymbolTable",
        nullptr,
        sizeof(SymbolTable),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) symtable_cleanup,
        nullptr,
        (CompareOp) symtable_compare,
        symtable_is_true,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::lang::compiler::type_symtable_ = &SymbolTableType;

bool argon::lang::compiler::SymbolTableEnterSub(SymbolTable **symt) {
    auto *st = SymbolTableNew(*symt);

    if (st == nullptr || !ListAppend((*symt)->namespaces, st)) {
        Release(st);
        return false;
    }

    Release(st);
    *symt = st;
    return true;
}

SymbolTable *argon::lang::compiler::SymbolTableNew(SymbolTable *prev) {
    auto *symt = ArObjectNew<SymbolTable>(RCType::INLINE, type_symtable_);

    if (symt != nullptr) {
        symt->prev = prev;

        if ((symt->map = MapNew()) == nullptr) {
            Release(symt);
            return nullptr;
        }

        if ((symt->namespaces = ListNew()) == nullptr) {
            Release(symt);
            return nullptr;
        }

        symt->nested = 0;
        if (symt->prev != nullptr)
            symt->nested = prev->nested + 1;
    }

    return symt;
}

Symbol *argon::lang::compiler::SymbolTableInsert(SymbolTable *symt, String *name, SymbolType kind, bool *out_inserted) {
    Symbol *sym;
    bool inserted = false;

    if ((sym = (Symbol *) MapGetNoException(symt->map, name)) != nullptr) {
        if (sym->kind != SymbolType::UNKNOWN) {
            Release(sym);
            return (Symbol *) ErrorFormat(type_compile_error_,
                                          "redeclaration of '%s(%s)' previously known as '%s %s'",
                                          name->buffer, SymbolType2Name[(int) kind],
                                          SymbolType2Name[(int) sym->kind], name->buffer);
        }
    } else {
        if ((sym = SymbolNew()) == nullptr)
            return nullptr;

        if (!MapInsert(symt->map, name, sym)) {
            Release(sym);
            return nullptr;
        }

        sym->kind = kind;
        sym->nested = symt->nested;

        inserted = true;
    }

    if (out_inserted != nullptr)
        *out_inserted = inserted;

    return sym;
}

Symbol *argon::lang::compiler::SymbolTableInsertNs(SymbolTable *symt, String *name, SymbolType kind) {
    SymbolTable *st;
    Symbol *sym;
    bool inserted;

    if ((sym = SymbolTableInsert(symt, name, kind, &inserted)) == nullptr)
        return nullptr;

    if (!inserted) {
        Release(sym);
        return (Symbol *) ErrorFormat(type_compile_error_, "%s %s already defined",
                                      SymbolType2Name[(int) sym->kind], name->buffer);
    }

    if ((st = SymbolTableNew(nullptr)) == nullptr) {
        Release(sym);
        return nullptr;
    }

    st->nested = symt->nested + 1;

    sym->declared = true;
    sym->symt = st;

    return sym;
}

Symbol *argon::lang::compiler::SymbolTableLookup(SymbolTable *symt, String *name) {
    Symbol *sym = nullptr;

    for (SymbolTable *cur = symt; cur != nullptr; cur = cur->prev) {
        if ((sym = (Symbol *) MapGetNoException(cur->map, name)) != nullptr)
            break;
    }

    return sym;
}

void argon::lang::compiler::SymbolTableExitSub(SymbolTable **symt) {
    SymbolTable *current = *symt;

    if (current->prev != nullptr)
        *symt = current->prev;
}