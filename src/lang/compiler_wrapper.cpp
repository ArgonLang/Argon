// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/error.h>

#include <lang/parser/parser.h>
#include <lang/compiler.h>

#include <vm/runtime.h>

#include "compiler_wrapper.h"

using namespace argon::lang;
using namespace argon::lang::parser;
using namespace argon::vm::datatype;

Code *CompilerWrapper::Compile(const char *file_name, scanner::Scanner &scanner) {
    Compiler compiler;
    Parser parser(file_name, scanner);
    File *ast;

    if ((ast = parser.Parse()) == nullptr)
        return nullptr;

    auto *code = compiler.Compile(ast);

    Release(ast);

    return code;
}

Code *CompilerWrapper::Compile(const char *file_name, const char *code, unsigned long code_sz) {
    scanner::Scanner scanner(code, code_sz);
    return this->Compile(file_name, scanner);
}

Code *CompilerWrapper::Compile(const char *file_name, FILE *fd) {
    scanner::Scanner scanner(nullptr, nullptr, fd);
    return this->Compile(file_name, scanner);
}

Code *CompilerWrapper::Compile(const char *file_name, String *code) {
    scanner::Scanner scanner((const char *) ARGON_RAW_STRING(code), ARGON_RAW_STRING_LENGTH(code));
    return this->Compile(file_name, scanner);
}
