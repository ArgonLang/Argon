// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMBOL_TABLE_H_
#define ARGON_LANG_SYMBOL_TABLE_H_

#include <unordered_map>
#include <memory>

namespace lang {

    struct Symbol {
        const std::string name;
        const class SymbolTable *table;
        unsigned int id = 0;
        bool declared = false;

        explicit Symbol(std::string name, class SymbolTable *table) : name(std::move(name)), table(table) {}
    };

    using SymUptr = std::unique_ptr<Symbol>;

    class StringPtrHashWrapper {
    public:
        size_t operator()(const std::string *str) const { return std::hash<std::string>()(*str); }
    };

    class StringPtrEqualToWrapper {
    public:
        bool operator()(const std::string *str, const std::string *other) const { return *str == *other; }
    };

    class SymbolTable {
        std::unordered_map<std::string *, SymUptr, StringPtrHashWrapper, StringPtrEqualToWrapper> map_;
        SymbolTable *prev_ = nullptr;
    public:
        const std::string name;
        const unsigned short level;

        SymbolTable(std::string name, unsigned short level);

        explicit SymbolTable(std::string name);

        SymbolTable *NewScope(std::string table_name);

        Symbol *Insert(const std::string &sym_name);

        Symbol *Lookup(const std::string &sym_name);
    };

    using SymTUptr = std::unique_ptr<SymbolTable>;

} // namespace lang

#endif // !ARGON_LANG_SYMBOL_TABLE_H_
