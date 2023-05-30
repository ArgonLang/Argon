// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/error.h>

#include <argon/lang/symt.h>

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

bool argon::lang::SymbolNewSub(SymbolT *table) {
    if (table->sub == nullptr) {
        table->sub = ListNew();
        if (table->sub == nullptr)
            return false;
    }

    auto *subt = SymbolNew(nullptr);
    if (subt == nullptr)
        return false;

    if (!ListAppend(table->sub, (ArObject *) subt)) {
        Release(subt);
        return false;
    }

    Release(subt);

    subt->type = SymbolType::NESTED;
    subt->nested = table->nested;

    table->nested_stack = subt;
    return true;
}

SymbolT *argon::lang::SymbolInsert(SymbolT *table, String *name, SymbolType type) {
    SymbolT *sym;

    if ((sym = (SymbolT *) DictLookup(table->stable, (ArObject *) name)) != nullptr) {
        if (sym->type != SymbolType::UNKNOWN && sym->declared) {
            ErrorFormat("RedeclarationError","redeclaration of '%s' as '%s %s' previously known as '%s %s'",
                        ARGON_RAW_STRING(name),
                        SymbolType2Name[(int) type],
                        ARGON_RAW_STRING(name),
                        SymbolType2Name[(int) sym->type],
                        ARGON_RAW_STRING(name));

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
    }

    sym->back = table;
    sym->type = type;
    sym->nested = table->nested;

    if (table->type != SymbolType::MODULE)
        sym->nested++;

    return sym;
}

SymbolT *argon::lang::SymbolLookup(const SymbolT *table, String *name) {
    SymbolT *sym;

    for (const SymbolT *nested = table->nested_stack; nested != nullptr; nested = nested->back) {
        if ((sym = (SymbolT *) DictLookup(nested->stable, (ArObject *) name)) != nullptr)
            return sym;
    }

    if ((sym = (SymbolT *) DictLookup(table->stable, (ArObject *) name)) != nullptr)
        return sym;


    return nullptr;
}

SymbolT *argon::lang::SymbolNew(String *name) {
    auto *symt = MakeObject<SymbolT>(type_symt_);

    if (symt != nullptr) {
        symt->back = nullptr;
        symt->nested_stack = nullptr;

        symt->name = IncRef(name);

        if ((symt->stable = DictNew()) == nullptr) {
            Release(symt);
            return nullptr;
        }

        symt->sub = nullptr;

        symt->id = -1;

        symt->type = SymbolType::MODULE;

        symt->nested = 0;

        symt->declared = false;

        symt->free = false;
    }

    return symt;
}

void argon::lang::SymbolExitSub(SymbolT *table) {
    if (table->nested_stack != nullptr)
        table->nested_stack = table->nested_stack->back;
}