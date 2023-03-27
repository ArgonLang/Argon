// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_PARSERERR_H_
#define ARGON_LANG_PARSER_PARSERERR_H_

#include <stdexcept>

namespace argon::lang::parser {
    class DatatypeException : public std::exception {
    };

    class ParserException : public std::exception {
        const char *message_;

    public:
        explicit ParserException(const char *message) : message_(message) {}

        [[nodiscard]] const char *what() const noexcept override {
            return this->message_;
        }
    };

    class ScannerException : public std::exception {
    };
} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSERERR_H_
