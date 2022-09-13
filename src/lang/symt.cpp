// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "symt.h"

using namespace argon::vm::datatype;
using namespace argon::lang;

const argon::vm::datatype::TypeInfo SymbolTType = {
        AROBJ_HEAD_INIT_TYPE,
        "SymbolT",
        nullptr,
        nullptr,
        sizeof(SymbolT),
        TypeInfoFlags::BASE,
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
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const argon::vm::datatype::TypeInfo *argon::lang::type_symt_ = &SymbolTType;

SymbolT *argon::lang::SymbolInsert(SymbolT *table, String *name, bool *out_inserted, SymbolType type) {
    SymbolT *sym;

    bool inserted = false;

    if ((sym = (SymbolT *) DictLookup(table->stable, (ArObject *) name)) != nullptr) {
        if (sym->type != SymbolType::UNKNOWN && sym->declared) {
            // TODO redeclaration error!
            Release(sym);
            return nullptr;
        }
    } else {
        if ((sym = SymbolNew(name)) == nullptr)
            return nullptr;

        if (!DictInsert(table->stable, (ArObject *) name, (ArObject *) sym)) {
            Release(sym);
            return nullptr;
        }

        inserted = true;
    }

    sym->type = type;
    sym->nested = table->nested;

    if (out_inserted != nullptr)
        *out_inserted = inserted;

    return sym;
}

SymbolT *argon::lang::SymbolNew(String *name) {
    auto *symt = MakeObject<SymbolT>(type_symt_);

    if (symt != nullptr) {
        symt->back = nullptr;

        symt->name = IncRef(name);

        if ((symt->stable = DictNew()) == nullptr) {
            Release(symt);
            return nullptr;
        }

        symt->sub = nullptr;

        symt->id = (ArSize) symt;

        symt->type = SymbolType::MODULE;

        symt->nested = 0;

        symt->declared = false;

        symt->free = false;
    }

    return symt;
}