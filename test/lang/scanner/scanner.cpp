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

TEST(Scanner, BinaryNumber) {
    Scanner scanner("0b1010 0B101");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER_BIN, 0, 1, 1, 6, 7, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER_BIN, 7, 8, 1, 12, 13, 1));
}

TEST(Scanner, OctalNumber) {
    Scanner scanner("0o23423 0O02372");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER_OCT, 0, 1, 1, 7, 8, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER_OCT, 8, 9, 1, 15, 16, 1));
}

TEST(Scanner, HexNumber) {
    Scanner scanner("0xaba12 0X19Fa");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER_HEX, 0, 1, 1, 7, 8, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::NUMBER_HEX, 8, 9, 1, 14, 15, 1));
}

TEST(Scanner, Word) {
    Scanner scanner("vax v4r v_48_ __private_var__ b as assert");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::IDENTIFIER, 0, 1, 1, 4, 5, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::IDENTIFIER, 4, 5, 1, 8, 9, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::IDENTIFIER, 8, 9, 1, 14, 15, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::IDENTIFIER, 14, 15, 1, 30, 31, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::IDENTIFIER, 30, 31, 1, 32, 33, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::KW_AS, 32, 33, 1, 35, 36, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::KW_ASSERT, 35, 36, 1, 41, 42, 1));
}

TEST(Scanner, LiteralByteString) {
    Scanner scanner(R"(b"ByteString" b"Ignore\u2342Unico\U00002312de" b"�")");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::BYTE_STRING, 0, 1, 1, 13, 14, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::BYTE_STRING, 14, 15, 1, 46, 47, 1));

    ASSERT_FALSE(scanner.NextToken(&token));
}

TEST(Scanner, RawString) {
    Scanner scanner(R"(r"plain" r#"plain hash"# r###"multiple hash"### r##"internal " h"#sh#"## r####"New "###
Line!
rString"#### r"")");
    Token token{};

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::RAW_STRING, 0, 1, 1, 8, 9, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::RAW_STRING, 9, 10, 1, 24, 25, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::RAW_STRING, 25, 26, 1, 47, 48, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::RAW_STRING, 48, 49, 1, 72, 73, 1));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::RAW_STRING, 73, 74, 1, 106, 13, 3));

    ASSERT_TRUE(scanner.NextToken(&token));
    ASSERT_TRUE(TkEqual(&token, TokenType::RAW_STRING, 107, 14, 3, 110, 17, 3));

    scanner = Scanner("r\"Error!");
    ASSERT_FALSE(scanner.NextToken(&token));

    scanner = Scanner("r#\"Error!\"");
    ASSERT_FALSE(scanner.NextToken(&token));

    scanner = Scanner("r##\"Error!##");
    ASSERT_FALSE(scanner.NextToken(&token));

    scanner = Scanner("r##Error!\"##");
    ASSERT_FALSE(scanner.NextToken(&token));
}