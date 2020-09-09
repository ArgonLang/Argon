// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "symbol_table.h"

using namespace argon::lang::symbol_table;

SymbolTable::SymbolTable(std::string name, unsigned short level) : name(std::move(name)), level(level) {
    this->stack_map_ = new(MapStack);
}

SymbolTable::SymbolTable(std::string name) : SymbolTable(std::move(name), 0) {}

SymbolTable::~SymbolTable() {
    MapStack *tmp;

    for (MapStack *iter = this->stack_map_; iter != nullptr; iter = tmp) {
        tmp = iter->prev;
        delete iter;
    }
}

Symbol *SymbolTable::Insert(const std::string &sym_name) {
    SymUptr symbol;
    Symbol *raw;

    if (this->stack_map_->map.find((std::string *) &sym_name) != this->stack_map_->map.end())
        return nullptr;

    symbol = std::make_unique<Symbol>(sym_name, this->stack_map_->nested);
    raw = symbol.get();

    this->stack_map_->map[(std::string *) &sym_name] = std::move(symbol);

    return raw;
}

Symbol *SymbolTable::Lookup(const std::string &sym_name) {
    for (MapStack *sub = this->stack_map_; sub != nullptr; sub = sub->prev) {
        auto itm = sub->map.find((std::string *) &sym_name);
        if (itm != sub->map.end())
            return itm->second.get();
    }

    return nullptr;
}

void SymbolTable::EnterSubScope() {
    auto ms = new MapStack();
    ms->prev = this->stack_map_;
    ms->nested = this->stack_map_->nested + 1;
    this->stack_map_ = ms;
}

void SymbolTable::ExitSubScope() {
    if (this->stack_map_->prev != nullptr) {
        auto ms = this->stack_map_->prev;
        delete this->stack_map_;
        this->stack_map_ = ms;
    }
}
