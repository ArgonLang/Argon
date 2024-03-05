// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/compiler2/compiler2.h>
#include <argon/lang/parser2/parser2.h>

#include <argon/lang/compiler_wrapper.h>

using namespace argon::lang;
using namespace argon::lang::compiler2;
using namespace argon::lang::parser2;
using namespace argon::vm::datatype;

CompilerWrapper::CompilerWrapper(int level) {
    this->level_ = compiler2::OptimizationLevel::OFF;

    if (level > 0 && level < 4)
        this->level_ = (compiler2::OptimizationLevel) level;
}

Code *CompilerWrapper::Compile(const char *file_name, scanner::Scanner &scanner) {
    Parser parser(file_name, scanner);

    auto *ast = parser.Parse();
    if (ast == nullptr)
        return nullptr;

    Compiler compiler(this->level_);

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
