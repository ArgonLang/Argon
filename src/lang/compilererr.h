// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSERERR_H_
#define ARGON_LANG_PARSERERR_H_

#include <stdexcept>

namespace argon::lang {
    class DatatypeException : public std::exception {
    };

    class CompilerException : public std::exception {
        const char *message_;

    public:
        explicit CompilerException(const char *message) : message_(message) {}

        [[nodiscard]] const char *what() const noexcept override {
            return this->message_;
        }
    };

} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSERERR_H_
