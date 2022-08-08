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
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_FILE, 0, 1, 1, 0, 1, 1));
}

TEST(Scanner, NewLine) {
    Scanner scanner("\n\n");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 0, 1, 1, 2, 1, 3));
}

TEST(Scanner, Spaces) {
    Scanner scanner("\n   \n\n \n");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 0, 1, 1, 1, 1, 2));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 4, 4, 2, 6, 1, 4));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_LINE, 7, 2, 4, 8, 1, 5));
}

TEST(Scanner, Numbers) {
    Scanner scanner("0 000123 123 010697 1");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER, 0, 1, 1, 1, 2, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER, 2, 3, 1, 8, 9, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER, 9, 10, 1, 12, 13, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER, 13, 14, 1, 19, 20, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER, 20, 21, 1, 21, 22, 1));
}

TEST(Scanner, Decimals) {
    Scanner scanner("0. 2.3 1234.003 00000.3 .1");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::DECIMAL, 0, 1, 1, 2, 3, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::DECIMAL, 3, 4, 1, 6, 7, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::DECIMAL, 7, 8, 1, 15, 16, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::DECIMAL, 16, 17, 1, 23, 24, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::DECIMAL, 24, 25, 1, 26, 27, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::END_OF_FILE, 26, 27, 1, 26, 27, 1));
}