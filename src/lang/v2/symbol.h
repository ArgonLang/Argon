// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMBOL_H_
#define ARGON_LANG_SYMBOL_H_

#include <string>
#include <memory>

#include "basicblock.h"

namespace argon::lang {

    enum class SymbolType {
        CONSTANT,
        LABEL,
        UNKNOWN,
        VARIABLE
    };

    class Symbol {
        const std::string name;
        const unsigned short nested = 0;
        unsigned int id = 0;

        SymbolType type;

        bool declared = false;
        bool free = false;

        BasicBlock *blocks = nullptr;

    public:
        Symbol(std::string name, unsigned short nested) : name(std::move(name)),
                                                          type(SymbolType::UNKNOWN),
                                                          nested(nested) {}
    };

    using SymbolUptr = std::unique_ptr<Symbol>;

} // namespace argon::lang

#endif // !ARGON_LANG_SYMBOL_H_
