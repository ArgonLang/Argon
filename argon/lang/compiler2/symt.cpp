// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/error.h>

#include <argon/lang/compiler2/symt.h>

#include <argon/vm/memory/memory.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;

bool symt_dtor(SymbolT *self){
    Release(self->name);
    Release(self->symbols);
    Release(self->subs);

    return true;
}

const argon::vm::datatype::TypeInfo SymbolTType = {
        AROBJ_HEAD_INIT_TYPE,
        "SymbolT",
        nullptr,
        nullptr,
        sizeof(SymbolT),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) symt_dtor,
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
const TypeInfo *argon::lang::compiler2::type_symbol_t_ = &SymbolTType;

bool SymbolT::MergeNested() const {
    SymbolT *tmp;

    if (this->stack == nullptr)
        return true;

    for (auto *cursor = this->stack->symbols->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        const SymbolT *subt = (SymbolT *) cursor->value;

        if ((tmp = (SymbolT *) DictLookup(this->symbols, cursor->key)) != nullptr) {
            if (tmp->type != subt->type && tmp->type != SymbolType::UNKNOWN) {
                ErrorFormat("SymbolError", "redeclaration of '%s' as '%s %s' previously known as '%s %s'",
                            ARGON_RAW_STRING(subt->name),
                            SymbolTypeName[(int) subt->type],
                            ARGON_RAW_STRING(subt->name),
                            SymbolTypeName[(int) tmp->type],
                            ARGON_RAW_STRING(subt->name));

                return false;
            }

            tmp->declared = subt->declared;
            tmp->type = subt->type;

            Release(tmp);
            continue;
        }

        if (!DictInsert(this->symbols, cursor->key, cursor->value))
            return false;
    }

    return true;
}

bool SymbolT::NewNestedTable() {
    if (this->subs == nullptr) {
        this->subs = ListNew();

        if (this->subs == nullptr)
            return false;
    }

    auto *subt = SymbolTableNew(this->stack, nullptr, SymbolType::NESTED);
    if (subt == nullptr)
        return false;

    subt->nested = this->nested + 1;

    if (!ListAppend(this->subs, (ArObject *) subt)) {
        Release(subt);

        return false;
    }

    Release(subt);

    this->stack = subt;

    return true;
}

SymbolT *SymbolT::SymbolInsert(String *s_name, SymbolType s_type, bool freevar) {
    SymbolT *sym = SymbolLookup(s_name, true);
    SymbolT *target = this;

    if (!freevar && this->stack != nullptr)
        target = this->stack;

    if (sym != nullptr) {
        if (sym->type != SymbolType::UNKNOWN && sym->declared) {
            ErrorFormat("SymbolError", "redeclaration of '%s' as '%s %s' previously known as '%s %s'",
                        ARGON_RAW_STRING(s_name),
                        SymbolTypeName[(int) s_type],
                        ARGON_RAW_STRING(s_name),
                        SymbolTypeName[(int) sym->type],
                        ARGON_RAW_STRING(s_name));

            Release(sym);

            return nullptr;
        }
    } else {
        if ((sym = SymbolNew(s_name, s_type)) == nullptr)
            return nullptr;

        if (!DictInsert(target->symbols, (ArObject *) s_name, (ArObject *) sym)) {
            Release(sym);

            return nullptr;
        }
    }

    //sym->back = target;
    sym->nested = target->nested;

    return sym;
}

SymbolT *SymbolT::SymbolLookup(const String *s_name, bool local) const {
    SymbolT *sym;

    if (local) {
        if (this->stack != nullptr) {
            sym = (SymbolT *) DictLookup(this->stack->symbols, (ArObject *) s_name);
            return sym;
        }

        return (SymbolT *) DictLookup(this->symbols, (ArObject *) s_name);
    }

    for (const SymbolT *cursor = this->stack; cursor != nullptr; cursor = cursor->back) {
        if ((sym = (SymbolT *) DictLookup(cursor->symbols, (ArObject *) s_name)) != nullptr)
            return sym;
    }

    if ((sym = (SymbolT *) DictLookup(this->symbols, (ArObject *) s_name)) != nullptr)
        return sym;

    return nullptr;
}

SymbolT *argon::lang::compiler2::SymbolTableNew(SymbolT *prev, String *name, SymbolType type) {
    auto *symt = SymbolNew(name, type);

    if (symt != nullptr) {
        symt->back = prev;

        if ((symt->symbols = DictNew()) == nullptr) {
            Release(symt);

            return nullptr;
        }

        symt->nested = prev != nullptr ? prev->nested + 1 : 0;
    }

    return symt;
}

SymbolT *argon::lang::compiler2::SymbolNew(String *name, SymbolType type) {
    auto *symt = MakeObject<SymbolT>(type_symbol_t_);

    if (symt != nullptr) {
        auto *n_obj = (((unsigned char *) symt) + sizeof(ArObject));

        vm::memory::MemoryZero(n_obj, AR_GET_TYPE(symt)->size - sizeof(ArObject));

        symt->name = IncRef(name);
        symt->type = type;

        symt->id = -1;
    }

    return symt;
}

void argon::lang::compiler2::SymbolExitNested(SymbolT *symt, bool merge){
    if(symt->stack == nullptr)
        return;

    if(!merge){
        symt->stack = symt->stack->back;
        return;
    }

    symt->MergeNested();

    auto s_back = symt->stack->back;

    ListRemove(symt->subs, (ArObject *) symt->stack);

    symt->stack = s_back;
}