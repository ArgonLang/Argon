// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/scanner/scanner.h>
#include <lang/parser/parser.h>

using namespace argon::lang::scanner;
using namespace argon::lang::parser;

TEST(Parser, EmptyInput) {
    Scanner scanner("");
    Parser parser(&scanner, "<anonymous>");

    Node * node = parser.Parse();
}
