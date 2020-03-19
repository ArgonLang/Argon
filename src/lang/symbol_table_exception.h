// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SYMBOL_TABLE_EXCEPTION_H_
#define ARGON_LANG_SYMBOL_TABLE_EXCEPTION_H_

#include <exception>

namespace lang {
    class SymbolTableException : std::exception {
    public:
        const std::string errmsg;
        const std::string varname;

        explicit SymbolTableException(std::string what, std::string varname) : errmsg(std::move(what)),
                                                                               varname(std::move(varname)) {}

        [[nodiscard]] const char *what() const noexcept override {
            return this->errmsg.c_str();
        }
    };

    class SymbolAlreadyDefinedException : SymbolTableException {
    public:
        explicit SymbolAlreadyDefinedException(std::string varname) : SymbolTableException("symbol already defined",
                                                                                           std::move(varname)) {}
    };

} // namespace lang


#endif // !ARGON_LANG_SYMBOL_TABLE_EXCEPTION_H_
