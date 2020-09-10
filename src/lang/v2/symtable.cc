// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "symtable.h"

using namespace argon::lang;

SymTable::SymTable() : SymTable(0) {}

SymTable::SymTable(unsigned short level) : level(level) {
    this->nested_symt_ = new MapStack;
}

SymTable::~SymTable() {
    MapStack *tmp;

    for (MapStack *iter = this->nested_symt_; iter != nullptr; iter = tmp) {
        tmp = iter->prev;
        delete iter;
    }
}

Symbol *SymTable::Insert(const std::string &sym_name) {
    SymbolUptr symbol;
    Symbol *raw;

    if (this->nested_symt_->map.find((std::string *) &sym_name) != this->nested_symt_->map.end())
        return nullptr;

    symbol = std::make_unique<Symbol>(sym_name, this->nested_symt_->nested);
    raw = symbol.get();

    this->nested_symt_->map[(std::string *) &sym_name] = std::move(symbol);

    return raw;
}

Symbol *SymTable::Lookup(const std::string &sym_name) {
    for (MapStack *sub = this->nested_symt_; sub != nullptr; sub = sub->prev) {
        auto itm = sub->map.find((std::string *) &sym_name);

        if (itm != sub->map.end())
            return itm->second.get();
    }

    return nullptr;
}

void SymTable::EnterSub() {
    auto ms = new MapStack();
    ms->prev = this->nested_symt_;
    ms->nested = this->nested_symt_->nested + 1;
    this->nested_symt_ = ms;
}

void SymTable::ExitSub() {
    if (this->nested_symt_->prev != nullptr) {
        auto ms = this->nested_symt_->prev;
        delete this->nested_symt_;
        this->nested_symt_ = ms;
    }
}
