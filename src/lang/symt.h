// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMT_H_
#define ARGON_LANG_SYMT_H_

#include <vm/datatype/arobject.h>
#include <vm/datatype/arstring.h>
#include <vm/datatype/dict.h>
#include <vm/datatype/list.h>

namespace argon::lang {
    enum class SymbolType {
        CONSTANT,
        FUNC,
        GENERATOR,
        LABEL,
        MODULE,
        STRUCT,
        TRAIT,
        UNKNOWN,
        VARIABLE
    };

    struct SymbolT {
        AROBJ_HEAD;

        SymbolT *back;

        vm::datatype::String *name;

        vm::datatype::Dict *stable;

        vm::datatype::List *sub;

        vm::datatype::ArSize id;

        SymbolType type;

        unsigned short nested;

        bool declared;

        bool free;
    };
    extern const argon::vm::datatype::TypeInfo *type_symt_;

    SymbolT *SymbolInsert(SymbolT *table, vm::datatype::String *name, bool *out_inserted, SymbolType type);

    SymbolT *SymbolNew(vm::datatype::String *name);
}

#endif // !ARGON_LANG_SYMT_H_
