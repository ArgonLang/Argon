// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <cstring>

#include "token.h"

using namespace argon::lang::scanner;

Token::Token(TokenType type, Pos start, Pos end, long buflen, const unsigned char *buf) {
    this->type = type;
    this->start = start;
    this->end = end;
    this->buflen = buflen;
    this->buf = (unsigned char *) buf;
}

Token::~Token() {
    if (this->buf != nullptr) {
        argon::memory::Free(this->buf);
        this->buf = nullptr;
    }
}

bool Token::operator==(const Token &token) const {
    return this->type == token.type
           && this->start == token.start
           && this->end == token.end
           && this->buf == token.buf;
}

bool Token::Clone(Token &dest) const noexcept {
    dest.type = this->type;
    dest.start = this->start;
    dest.end = this->end;

    dest.buflen = this->buflen;

    if (this->buf != nullptr) {
        if ((dest.buf = (unsigned char *) argon::memory::Alloc(this->buflen)) == nullptr)
            return false;

        argon::memory::MemoryCopy(dest.buf, this->buf, this->buflen);
    }

    return true;
}

Token &Token::operator=(Token &&token) noexcept {
    this->type = token.type;
    this->start = token.start;
    this->end = token.end;

    if (this->buf != nullptr)
        argon::memory::Free(this->buf);

    this->buf = token.buf;
    this->buflen = token.buflen;

    token.buf = nullptr;

    return *this;
}
