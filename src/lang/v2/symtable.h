// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMTABLE_H_
#define ARGON_LANG_SYMTABLE_H_

#include <string>
#include <cstddef>
#include <unordered_map>

#include "symbol.h"

namespace argon::lang {

    class StringPtrHashWrapper {
    public:
        size_t operator()(const std::string *str) const { return std::hash<std::string>()(*str); }
    };

    class StringPtrEqualToWrapper {
    public:
        bool operator()(const std::string *str, const std::string *other) const { return *str == *other; }
    };

    struct MapStack {
        /* Map that contains association between string(key) and symbol (pointer to Symbol obj) */
        std::unordered_map<std::string *, SymbolUptr, StringPtrHashWrapper, StringPtrEqualToWrapper> map;

        /* Level of nesting */
        unsigned short nested = 0;

        /* Pointer to prev symbol table */
        MapStack *prev = nullptr;
    };

    class SymTable {
        MapStack *nested_symt_;
    public:

        const unsigned short level;

        SymTable();

        explicit SymTable(unsigned short level);

        ~SymTable();

        Symbol *Insert(const std::string &sym_name);

        Symbol *Lookup(const std::string &sym_name);

        void EnterSub();

        void ExitSub();
    };

} // namespace argon::lang

#endif // !ARGON_LANG_SYMTABLE_H_
