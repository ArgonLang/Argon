// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "compiler_exception.h"
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
    return this->Insert(sym_name, SymbolType::UNKNOWN, nullptr);
}

Symbol *SymTable::Insert(const std::string &sym_name, SymbolType type, bool *out_inserted) {
    bool inserted = true;
    SymbolUptr symbol;
    Symbol *raw;

    auto itm = this->nested_symt_->map.find((std::string *) &sym_name);

    if (itm != this->nested_symt_->map.end()) {
        auto &sym = itm->second;

        if (sym->declared) {
            std::string error;

            if (sym->type == SymbolType::VARIABLE)
                error = "redeclaration of variable: " + sym_name;
            else if (sym->type == SymbolType::CONSTANT)
                error = "redeclaration of variable '" + sym_name + "' previously known as: let " + sym_name +
                        " (constant)";
            else
                assert(false);

            throw RedeclarationException(error);
        }
        raw = itm->second.get();
        inserted = false;
    } else {
        symbol = std::make_unique<Symbol>(sym_name, this->nested_symt_->nested);
        raw = symbol.get();
        raw->type = type;
        this->nested_symt_->map[(std::string *) &symbol->name] = std::move(symbol);
    }

    if (out_inserted != nullptr)
        *out_inserted = inserted;

    if (type != SymbolType::UNKNOWN)
        raw->declared = true;

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
