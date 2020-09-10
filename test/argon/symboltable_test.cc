// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <lang/symtable.h>

using namespace argon::lang;

TEST(SymbolTable, Insert) {
    auto symt = std::make_shared<SymTable>();
    ASSERT_EQ(symt->Insert("var_a")->name, "var_a");
    ASSERT_EQ(symt->Insert("var_a"), nullptr);
}

TEST(SymbolTable, Lookup) {
    auto symt = std::make_shared<SymTable>();
    symt->Insert("var_a");
    symt->EnterSub();
    symt->Insert("var_b");
    symt->EnterSub();
    ASSERT_EQ(symt->Lookup("var_a")->name, "var_a");
    ASSERT_EQ(symt->Lookup("var_b")->name, "var_b");
    symt->ExitSub();
    symt->ExitSub();
    ASSERT_EQ(symt->Lookup("var_a")->name, "var_a");
    ASSERT_EQ(symt->Lookup("var_b"), nullptr);
}