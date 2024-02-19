// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_EXCEPTIONS_H_
#define ARGON_LANG_PARSER2_EXCEPTIONS_H_

#include "argon/vm/datatype/arstring.h"

#include "argon/lang/scanner/token.h"

#include <stdexcept>

namespace argon::lang {
    using namespace argon::vm::datatype;

    class CompilerException : public std::exception {
        String *str_ = nullptr;

        char *message_ = nullptr;
    public:
        explicit CompilerException(const char *message, ...) {
            va_list args;

            va_start(args, message);
            auto *str = StringFormat(message, args);
            va_end(args);

            if (str != nullptr)
                this->message_ = (char *) str->buffer;

            this->str_ = str;
        }

        ~CompilerException() override {
            Release(this->str_);
        }

        [[nodiscard]] const char *what() const noexcept override {
            return this->message_;
        }
    };

    class DatatypeException : public std::exception {
    };

    class ParserException : public std::exception {
        String *str_ = nullptr;

        char *message_ = nullptr;
    public:
        const scanner::Loc loc;

        explicit ParserException(const scanner::Loc loc, const char *message, ...) : loc(loc) {
            va_list args;

            va_start(args, message);
            auto *str = StringFormat(message, args);
            va_end(args);

            if (str != nullptr)
                this->message_ = (char *) str->buffer;

            this->str_ = str;
        }

        ~ParserException() override {
            Release(this->str_);
        }

        [[nodiscard]] const char *what() const noexcept override {
            return this->message_;
        }
    };

    class ScannerException : public std::exception {
    };

} // namespace argon::lang::parser2

#endif // ARGON_LANG_PARSER2_EXCEPTIONS_H_