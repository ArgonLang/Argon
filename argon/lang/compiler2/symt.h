// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_SYMT_H_
#define ARGON_LANG_COMPILER2_SYMT_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/dict.h>

namespace argon::lang::compiler2 {
    enum class SymbolType {
        CONSTANT,
        FUNC,
        GENERATOR,
        MODULE,
        NESTED,
        STRUCT,
        TRAIT,
        VARIABLE,
        UNKNOWN
    };

    static const char *SymbolTypeName[] = {
            "let",
            "function",
            "generator",
            "module",
            "",
            "struct",
            "trait",
            "var",
            ""
    };

    struct SymbolT {
        AROBJ_HEAD;

        SymbolT *back;

        SymbolT *stack;

        vm::datatype::String *name;

        vm::datatype::Dict *symbols;

        vm::datatype::List *subs;

        SymbolType type;

        short id;

        unsigned short nested;

        bool declared;

        bool free;

        bool MergeNested() const;

        bool NewNestedTable();

        SymbolT *SymbolInsert(vm::datatype::String *s_name, SymbolType s_type, bool freevar);

        inline SymbolT *SymbolInsert(vm::datatype::String *s_name, SymbolType s_type) {
            return SymbolInsert(s_name, s_type, false);
        }

        SymbolT *SymbolLookup(const vm::datatype::String *s_name, bool local) const;
    };

    _ARGONAPI extern const argon::vm::datatype::TypeInfo *type_symbol_t_;

    SymbolT *SymbolTableNew(SymbolT *prev, vm::datatype::String *name, SymbolType type);

    SymbolT *SymbolNew(vm::datatype::String *name, SymbolType type);

    void SymbolExitNested(SymbolT *symt, bool merge);

    inline void SymbolExitNested(SymbolT *symt) {
        SymbolExitNested(symt, false);
    }

} // argon::lang::compiler2

#endif // !ARGON_LANG_COMPILER2_SYMT_H_
