// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_EXCEPTION_H_
#define ARGON_LANG_COMPILER_EXCEPTION_H_

#include <exception>

namespace lang {
    class CompilerException : std::exception {
    public:
        ~CompilerException() override = default;
    };

    class MemoryException : CompilerException {
        const char *what_;
    public:
        explicit MemoryException(const char *what) : what_(what) {}

        [[nodiscard]] const char *what() const noexcept override {
            return this->what_;
        }
    };
} // namespace lang

#endif // !ARGON_LANG_COMPILER_EXCEPTION_H_
