// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/symbol_table.h>
#include <lang/symbol_table_exception.h>

using namespace lang;

TEST(SymbolTable, NewScope) {
    auto symt = std::make_shared<lang::SymbolTable>("main");
    auto fna = symt->NewScope("fun_a");

    ASSERT_EQ(symt->level, 0);
    ASSERT_EQ(fna->level, 1);
}

TEST(SymbolTable, Insert) {
    auto symt = std::make_shared<lang::SymbolTable>("main");
    ASSERT_EQ(symt->Insert("var_a")->name, "var_a");
    ASSERT_EQ(symt->Insert("var_a"), nullptr);
}

TEST(SymbolTable, Lookup) {
    auto symt = std::make_shared<lang::SymbolTable>("main");
    auto fna = symt->NewScope("fun_a");
    symt->Insert("var_a");
    ASSERT_EQ(fna->Lookup("var_a")->name, "var_a");
}

TEST(SymbolTable, LocalLookup) {
    auto symt = std::make_shared<lang::SymbolTable>("main");
    symt->Insert("var_a");

    ASSERT_EQ(symt->Lookup("var_a")->name, "var_a");
}