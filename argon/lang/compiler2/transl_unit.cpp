// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

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
