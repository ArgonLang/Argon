// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0


#ifndef ARGON_LANG_SYNTAX_EXCEPTION_H_
#define ARGON_LANG_SYNTAX_EXCEPTION_H_

#include <exception>
#include <lang/scanner/token.h>

namespace argon::lang {
    class SyntaxException : std::exception {
    public:
        const std::string errmsg;
        const scanner::Token token;

        explicit SyntaxException(std::string what, scanner::Token token) : errmsg(std::move(what)),
                                                                           token(std::move(token)) {}

        const char *what() const noexcept override {
            return this->errmsg.c_str();
        }
    };
} // namespace lang

#endif // !ARGON_LANG_SYNTAX_EXCEPTION_H_
