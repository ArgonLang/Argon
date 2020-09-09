// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMT_SYMBOL_H_
#define ARGON_LANG_SYMT_SYMBOL_H_

#include <string>
#include <memory>

namespace argon::lang::symbol_table {
    struct Symbol {
        const std::string name;
        const unsigned int nested;
        unsigned int id = 0;
        bool declared = false;
        bool free = false;

        explicit Symbol(std::string name, unsigned int nested) : name(std::move(name)), nested(nested) {}
    };

    using SymUptr = std::unique_ptr<Symbol>;
}

#endif // !ARGON_LANG_SYMT_SYMBOL_H_
