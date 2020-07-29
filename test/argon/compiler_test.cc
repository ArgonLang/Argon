// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/compiler.h>

#include <object/datatype/bool.h>

TEST(Compiler, SimpleExpr) {
    auto a = argon::object::True;


    auto source = std::istringstream("1+1");
    lang::Compiler compiler;
    compiler.Compile(&source);
}