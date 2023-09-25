// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMT_H_
#define ARGON_LANG_SYMT_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/list.h>

namespace argon::lang {
    enum class SymbolType {
        CONSTANT,
        FUNC,
        GENERATOR,
        LABEL,
        MODULE,
        NESTED,
        STRUCT,
        TRAIT,
        UNKNOWN,
        VARIABLE
    };

    static const char *SymbolType2Name[] = {
        "let",
        "func",
        "generator",
        "LABEL",
        "module",
        "NESTED",
        "struct",
        "trait",
        "UNKNOWN",
        "var"
    };

    struct SymbolT {
        AROBJ_HEAD;

        SymbolT *back;

        SymbolT *nested_stack;

        vm::datatype::String *name;

        vm::datatype::Dict *stable;

        vm::datatype::List *sub;

        vm::datatype::ArSSize id;

        SymbolType type;

        unsigned short nested;

        bool declared;

        bool free;
    };
    _ARGONAPI extern const argon::vm::datatype::TypeInfo *type_symt_;

    bool SymbolNewSub(SymbolT *table);

    SymbolT *SymbolInsert(SymbolT *table, vm::datatype::String *name, SymbolType type);

    SymbolT *SymbolLookup(const SymbolT *table, vm::datatype::String *name);

    SymbolT *SymbolNew(vm::datatype::String *name, SymbolType type);

    SymbolT *SymbolTableNew(SymbolT *prev, vm::datatype::String *name, SymbolType type);

    void SymbolExitSub(SymbolT *table);
}

#endif // !ARGON_LANG_SYMT_H_
