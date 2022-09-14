// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_WRAPPER_H_
#define ARGON_LANG_COMPILER_WRAPPER_H_

#include <string>

#include <vm/datatype/arstring.h>
#include <vm/datatype/code.h>

#include <lang/scanner/scanner.h>

namespace argon::lang {
    class CompilerWrapper {
    public:
        vm::datatype::Code *Compile(const char *file_name, scanner::Scanner &scanner);

        vm::datatype::Code *Compile(const char *file_name, const char *code, unsigned long code_sz);

        vm::datatype::Code *Compile(const char *file_name, FILE *fd);

        vm::datatype::Code *Compile(const char *file_name, vm::datatype::String *code);

        vm::datatype::Code *Compile(const char *file_name, const std::string &code) {
            return this->Compile(file_name, code.c_str(), code.length());
        }

        vm::datatype::Code *Compile(const char *file_name, const char *code) {
            return this->Compile(file_name, code, strlen(code));
        }
    };
}

#endif // !ARGON_LANG_COMPILER_WRAPPER_H_
