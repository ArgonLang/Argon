// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <lang/scanner/scanner.h>
#include <lang/scanner/token.h>

TEST(Scanner, EmptyInput) {
	auto source = std::istringstream("");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken().type, lang::scanner::TokenType::END_OF_FILE);
}

TEST(Scanner, NewLine) {
	auto source = std::istringstream("\n\n");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 0, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_FILE, 0, 2, ""));
}

TEST(Scanner, Spaces) {
	auto source = std::istringstream("\n   \n\n \n");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 0, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 3, 1, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 1, 3, ""));
}

TEST(Scanner, BinaryNumber) {
	auto source = std::istringstream("0b1010 0B101");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_BIN, 0, 0, "1010"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_BIN, 7, 0, "101"));
}

TEST(Scanner, OctalNumber) {
	auto source = std::istringstream("0o23423 0O02372");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_OCT, 0, 0, "23423"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_OCT, 8, 0, "02372"));
}

TEST(Scanner, HexNumber) {
	auto source = std::istringstream("0xaba12 0X19Fa");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_HEX, 0, 0, "aba12"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_HEX, 8, 0, "19Fa"));
}

TEST(Scanner, Number) {
	auto source = std::istringstream("123 010697 1 0 22.3 6.");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 0, 0, "123"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 4, 0, "10697"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 11, 0, "1"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 13, 0, "0"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DECIMAL, 15, 0, "22.3"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DECIMAL, 20, 0, "6."));
}

TEST(Scanner, Word) {
	auto source = std::istringstream("var v4r v_48_ __private_var__");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 0, 0, "var"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 4, 0, "v4r"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 8, 0, "v_48_"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 14, 0, "__private_var__"));
}

TEST(Scanner, Punctuation) {
	auto source = std::istringstream("()+ -% &  *./:;< => ][  #^|}{ ~");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::LEFT_ROUND, 0, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::RIGHT_ROUND, 1, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::PLUS, 2, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::MINUS, 4, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::PERCENT, 5, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::AMPERSAND, 7, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::ASTERISK, 10, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DOT, 11, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::FRACTION_SLASH, 12, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::COLON, 13, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::SEMICOLON, 14, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::LESS, 15, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::EQUAL, 17, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::GREATER, 18, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::RIGHT_SQUARE, 20, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::LEFT_SQUARE, 21, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::HASH, 24, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::CARET, 25, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::PIPE, 26, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::RIGHT_BRACES, 27, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::LEFT_BRACES, 28, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::TILDE, 30, 0, ""));
}

TEST(Scanner, CompoundSyms) {
	auto source = std::istringstream("&& || >= <= != ... .. .");
	lang::scanner::Scanner scanner(source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::AND, 0, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::OR, 3, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::GREATER_EQ, 6, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::LESS_EQ, 9, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NOT_EQUAL, 12, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::ELLIPSIS, 15, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DOT, 19, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DOT, 20, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DOT, 22, 0, ""));
}