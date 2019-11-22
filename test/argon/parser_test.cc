// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <lang/parser.h>

using namespace lang;

TEST(Parser, Literals) {
    auto source = std::istringstream("\"HelloWorld\"");
    Parser p(&source);
}