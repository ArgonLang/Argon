// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/scanner/scanner2.h>
#include <lang/parser/parser.h>
#include <lang/compiler/compiler.h>

using namespace argon::lang::scanner2;
using namespace argon::lang::parser;
using namespace argon::lang::compiler;

TEST(Compiler, Test) {
    Scanner scanner(R"(

)");
    Parser parser(&scanner, "<anonymous>");

    Compiler compiler;

    compiler.Compile(parser.Parse());
}
