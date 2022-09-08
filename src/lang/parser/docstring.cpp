// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "parsererr.h"
#include "docstring.h"

using namespace argon::lang::parser;
using namespace argon::lang::scanner;
using namespace argon::vm::datatype;

DocString *argon::lang::parser::DocStringDel(DocString *ds) {
    auto *prev = ds->prev;

    Release(ds->docstring);

    argon::vm::memory::Free(ds);

    return prev;
}

DocString *argon::lang::parser::DocStringNew(DocString *prev) {
    auto *ds = (DocString *) argon::vm::memory::Alloc(sizeof(DocString));

    if (ds != nullptr) {
        ds->prev = prev;
        ds->uninterrupted = true;
        ds->docstring = nullptr;
        ds->loc = {};
    }

    return ds;
}

void DocString::AddString(const Token &token) {
    assert(token.type == scanner::TokenType::COMMENT || token.type == scanner::TokenType::COMMENT_INLINE);

    auto *tmp = StringConcat(this->docstring, (const char *) token.buffer, token.length);
    if (tmp == nullptr)
        throw DatatypeException();

    Release(this->docstring);

    if (docstring == nullptr)
        this->loc.start = token.loc.start;

    this->docstring = tmp;

    this->loc.end = token.loc.end;
}