// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_WRAPPER_H_
#define ARGON_LANG_COMPILER_WRAPPER_H_

#include <string>

#include <object/datatype/code.h>
#include <object/datatype/String.h>

#include <lang/scanner/scanner2.h>

namespace argon::lang {
    class CompilerWrapper {
        object::Code *Compile(const char *file_name, scanner2::Scanner *scanner);

    public:
        object::Code *Compile(const char *file_name, const char *code, unsigned long code_sz);

        object::Code *Compile(const char *file_name, FILE *fd);

        object::Code *Compile(const char *file_name, object::String *code);

        object::Code *Compile(const char *file_name, const std::string &code) {
            return this->Compile(file_name, code.c_str(), code.length());
        }

        object::Code *Compile(const char *file_name, const char *code) {
            return this->Compile(file_name, code, strlen(code));
        }
    };
}


#endif // !ARGON_LANG_COMPILER_WRAPPER_H_
