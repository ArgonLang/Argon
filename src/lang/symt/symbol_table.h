// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMT_SYMBOL_TABLE_H_
#define ARGON_LANG_SYMT_SYMBOL_TABLE_H_

#include <list>
#include <unordered_map>
#include "symbol.h"

namespace argon::lang::symbol_table {
    class StringPtrHashWrapper {
    public:
        size_t operator()(const std::string *str) const { return std::hash<std::string>()(*str); }
    };

    class StringPtrEqualToWrapper {
    public:
        bool operator()(const std::string *str, const std::string *other) const { return *str == *other; }
    };

    struct MapStack {
        std::unordered_map<std::string *, SymUptr, StringPtrHashWrapper, StringPtrEqualToWrapper> map;
        unsigned short nested = 0;
        MapStack *prev = nullptr;
    };

    class SymbolTable {
        MapStack *stack_map_;
    public:
        const std::string name;
        const unsigned short level;

        explicit SymbolTable(std::string name);

        SymbolTable(std::string name, unsigned short level);

        ~ SymbolTable();

        Symbol *Insert(const std::string &sym_name);

        Symbol *Lookup(const std::string &sym_name);

        void EnterSubScope();

        void ExitSubScope();
    };

    using SymTUptr = std::unique_ptr<SymbolTable>;

} // namespace lang

#endif // !ARGON_LANG_SYMT_SYMBOL_TABLE_H_
