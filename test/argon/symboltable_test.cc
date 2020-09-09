// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/symt/symbol_table.h>

using namespace argon::lang;
using namespace symbol_table;


TEST(SymbolTable, Insert) {
    auto symt = std::make_shared<SymbolTable>("main");
    ASSERT_EQ(symt->Insert("var_a")->name, "var_a");
    ASSERT_EQ(symt->Insert("var_a"), nullptr);
}

TEST(SymbolTable, Lookup) {
    auto symt = std::make_shared<SymbolTable>("main");
    symt->Insert("var_a");
    symt->EnterSubScope();
    symt->Insert("var_b");
    symt->EnterSubScope();
    ASSERT_EQ(symt->Lookup("var_a")->name, "var_a");
    ASSERT_EQ(symt->Lookup("var_b")->name, "var_b");
    symt->ExitSubScope();
    symt->ExitSubScope();
    ASSERT_EQ(symt->Lookup("var_a")->name, "var_a");
    ASSERT_EQ(symt->Lookup("var_b"), nullptr);
}