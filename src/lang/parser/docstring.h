// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_DOCSTRING_H_
#define ARGON_LANG_PARSER_DOCSTRING_H_

#include <vm/datatype/arstring.h>

#include <lang/scanner/token.h>

namespace argon::lang::parser {

    struct DocString {
        DocString *prev;

        vm::datatype::String *docstring;

        scanner::Loc loc;

        bool uninterrupted;

        vm::datatype::String *Unwrap() {
            auto *t = this->docstring;

            this->docstring = nullptr;

            return t;
        }

        void AddString(const scanner::Token &token);
    };

    DocString *DocStringDel(DocString *ds);

    DocString *DocStringNew(DocString *prev);

} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_DOCSTRING_H_
