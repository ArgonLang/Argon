// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/scanner/scanner.h>
#include <lang/scanner/token.h>

TEST(Scanner, EmptyInput) {
	auto source = std::istringstream("");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken().type, lang::scanner::TokenType::END_OF_FILE);
}

TEST(Scanner, NewLine) {
	auto source = std::istringstream("\n\n");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 0, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_FILE, 0, 2, ""));
}

TEST(Scanner, Spaces) {
	auto source = std::istringstream("\n   \n\n \n");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 0, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 3, 1, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::END_OF_LINE, 1, 3, ""));
}

TEST(Scanner, BinaryNumber) {
	auto source = std::istringstream("0b1010 0B101");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_BIN, 0, 0, "1010"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_BIN, 7, 0, "101"));
}

TEST(Scanner, OctalNumber) {
	auto source = std::istringstream("0o23423 0O02372");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_OCT, 0, 0, "23423"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_OCT, 8, 0, "02372"));
}

TEST(Scanner, HexNumber) {
	auto source = std::istringstream("0xaba12 0X19Fa");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_HEX, 0, 0, "aba12"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER_HEX, 8, 0, "19Fa"));
}

TEST(Scanner, Number) {
	auto source = std::istringstream("123 010697 1 0 22.3 6.");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 0, 0, "123"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 4, 0, "10697"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 11, 0, "1"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::NUMBER, 13, 0, "0"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DECIMAL, 15, 0, "22.3"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::DECIMAL, 20, 0, "6."));
}

TEST(Scanner, Word) {
	auto source = std::istringstream("var v4r v_48_ __private_var__ byte b");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 0, 0, "var"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 4, 0, "v4r"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 8, 0, "v_48_"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 14, 0, "__private_var__"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 30, 0, "byte"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::WORD, 35, 0, "b"));
}

TEST(Scanner, Punctuation) {
	auto source = std::istringstream("()+ -% &  *./:;< => ][  #^|}{ ~");
	lang::scanner::Scanner scanner(&source);
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
	lang::scanner::Scanner scanner(&source);
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

TEST(Scanner, String) {
	auto source = std::istringstream(R"("" "simple string" "\\" "Hello\"escaped\"")");
	lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 1, 0, ""));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 4, 0, "simple string"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 20, 0, "\\"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 25, 0, "Hello\"escaped\""));
}

TEST(Scanner, StringEscape) {
	auto source = std::istringstream(R"("bell\a" "\x7b" "\0" "\1" "\41" "\234" "\u0024" "\u03a3" "\u0939" "\U00010348" "Ignore\\"")");
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	lang::scanner::Scanner scanner(&source);

	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 1, 0, "bell\a"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 10, 0, "{"));
	ASSERT_EQ(scanner.NextToken().value[0], 0x00);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 22, 0, "\1"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 27, 0, "\41"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 33, 0, "\234"));
	
	// Unicode UTF-8
	ASSERT_EQ(scanner.NextToken() , lang::scanner::Token(lang::scanner::TokenType::STRING, 40, 0, "\x24"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 49, 0, converter.to_bytes(L"\u03a3")));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 58, 0, converter.to_bytes(L"\u0939")));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 67, 0, "\xF0\x90\x8D\x88"));

	// Ignore Escape
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::STRING, 80, 0, "Ignore\\"));
}

TEST(Scanner, UnterminatedString) {
	auto source = std::istringstream("\"hello");
	auto scanner = lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.NextToken().type, lang::scanner::TokenType::ERROR);

	source = std::istringstream("\"hello worl\nd\"");
	scanner = lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.NextToken().type, lang::scanner::TokenType::ERROR);
}

TEST(Scanner, bString) {
	auto source = std::istringstream(R"(b"ByteString" b"Ignore\u2342Unico\U00002312de" b"é")");
	auto scanner = lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::BYTE_STRING, 2, 0, "ByteString"));
	ASSERT_EQ(scanner.NextToken(), lang::scanner::Token(lang::scanner::TokenType::BYTE_STRING, 16, 0, "Ignore\\u2342Unico\\U00002312de"));
	ASSERT_EQ(scanner.NextToken().type, lang::scanner::TokenType::ERROR); // Extended ASCII not allowed here!
}
