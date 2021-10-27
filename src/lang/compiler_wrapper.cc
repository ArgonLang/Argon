// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>

#include <lang/parser/parser.h>
#include <lang/compiler/compiler.h>

#include "compiler_wrapper.h"

using namespace argon::lang;
using namespace argon::lang::parser;
using namespace argon::lang::compiler;
using namespace argon::object;

Code *CompilerWrapper::Compile(const char *file_name, scanner2::Scanner *scanner) {
    Parser parser(scanner, file_name);
    Compiler compiler;
    Code *code;
    File *ast;

    if ((ast = parser.Parse()) == nullptr) {
        if (scanner->status != scanner2::ScannerStatus::GOOD)
            return (Code *) ErrorFormat(type_syntax_error_, "%s", scanner->GetStatusMessage());
        return nullptr;
    }

    code = compiler.Compile(ast);
    Release(ast);

    return code;
}

Code *CompilerWrapper::Compile(const char *file_name, const char *code, unsigned long code_sz) {
    scanner2::Scanner scanner(code, code_sz);
    return this->Compile(file_name, &scanner);
}

Code *CompilerWrapper::Compile(const char *file_name, FILE *fd) {
    scanner2::Scanner scanner(fd, nullptr, nullptr);
    return this->Compile(file_name, &scanner);
}

Code *CompilerWrapper::Compile(const char *file_name, String *code) {
    scanner2::Scanner scanner((const char *) code->buffer, code->len);
    return this->Compile(file_name, &scanner);
}
