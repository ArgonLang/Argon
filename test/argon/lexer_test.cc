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