// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "symbol_table.h"

using namespace lang;

SymbolTable::SymbolTable(std::string name, SymTScope type, unsigned short level) : name(std::move(name)),
                                                                                   type(type), level(level) {}

SymbolTable::SymbolTable(std::string name, SymTScope type) : SymbolTable(std::move(name), type, 0) {}

SymbolTable *SymbolTable::NewScope(std::string table_name, SymTScope table_type) {
    auto table = new SymbolTable(std::move(table_name), table_type, this->level + 1);
    table->prev_ = this;
    return table;
}

Symbol *SymbolTable::Insert(const std::string &sym_name) {
    SymUptr symbol;
    Symbol *raw;

    if (this->map_.find((std::string *) &sym_name) != this->map_.end())
        return nullptr;

    symbol = std::make_unique<Symbol>(sym_name, this);
    raw = symbol.get();

    this->map_[(std::string *) &sym_name] = std::move(symbol);

    return raw;
}

Symbol *SymbolTable::Lookup(const std::string &sym_name) {
    for (SymbolTable *current = this; current != nullptr; current = current->prev_) {
        auto itm = current->map_.find((std::string *) &sym_name);
        if (itm != current->map_.end())
            return itm->second.get();
    }
    return nullptr;
}
