// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/scanner/scanner.h>

using namespace argon::lang::scanner;

bool TkEqual(const Token *tk, TokenType type, int so, int sc, int sl, int eo, int ec, int el) {
    return tk->type == type &&
           tk->loc.start.offset == so &&
           tk->loc.start.column == sc &&
           tk->loc.start.line == sl &&
           tk->loc.end.offset == eo &&
           tk->loc.end.column == ec &&
           tk->loc.end.line == el;
}

TEST(Scanner, EmptyInput) {
    Scanner scanner("");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_FILE, 0,1,1,0,1,1));
}

TEST(Scanner, NewLine) {
    Scanner scanner("\n\n");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 0,1,1,2,1,3));
}

TEST(Scanner, Spaces) {
    Scanner scanner("\n   \n\n \n");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 0,1,1,1,1,2));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 4,4,2,6,1,4));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 7,2,4,8,1,5));
}