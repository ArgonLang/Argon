// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/compiler.h>

TEST(Compiler, SimpleExpr) {
    auto source = std::istringstream("1+1");
    lang::Compiler compiler;
    compiler.Compile(&source);
}