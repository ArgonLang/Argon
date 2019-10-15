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