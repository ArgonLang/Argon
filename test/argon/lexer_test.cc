// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/scanner/scanner.h>
#include <lang/scanner/token.h>

TEST(Scanner, EmptyInput) {
	auto source = std::istringstream("");
	argon::lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::END_OF_FILE);
}

TEST(Scanner, NewLine) {
    auto source = std::istringstream("\n\n");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_LINE, 1, 3, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_FILE, 3, 3, ""));
}

TEST(Scanner, Spaces) {
    auto source = std::istringstream("\n   \n\n \n");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_LINE, 1, 2, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_LINE, 5, 7, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_LINE, 8, 9, ""));
}

TEST(Scanner, BinaryNumber) {
    auto source = std::istringstream("0b1010 0B101");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER_BIN, 1, 7, "1010"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER_BIN, 8, 13, "101"));
}

TEST(Scanner, OctalNumber) {
    auto source = std::istringstream("0o23423 0O02372");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER_OCT, 1, 8, "23423"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER_OCT, 9, 16, "02372"));
}

TEST(Scanner, HexNumber) {
    auto source = std::istringstream("0xaba12 0X19Fa");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER_HEX, 1, 8, "aba12"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER_HEX, 9, 15, "19Fa"));
}

TEST(Scanner, Number) {
    auto source = std::istringstream("123 010697 1 0 22.3 6.");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 1, 4, "123"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 5, 11, "10697"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 12, 13, "1"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 14, 15, "0"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::DECIMAL, 16, 20, "22.3"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::DECIMAL, 21, 23, "6."));
}

TEST(Scanner, Word) {
    auto source = std::istringstream("vax v4r v_48_ __private_var__ byte b");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 1, 4, "vax"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 5, 8, "v4r"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 9, 14, "v_48_"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 15, 30, "__private_var__"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 31, 35, "byte"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 36, 37, "b"));
}

TEST(Scanner, Delimiters) {
    auto source = std::istringstream("() ][ {}");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::LEFT_ROUND, 1, 2, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RIGHT_ROUND, 2, 3, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RIGHT_SQUARE, 4, 5, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::LEFT_SQUARE, 5, 6, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::LEFT_BRACES, 7, 8, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RIGHT_BRACES, 8, 9, ""));
}

TEST(Scanner, Punctuation) {
    auto source = std::istringstream("+ -% &  *./:;< = >  ^| ~,");
	argon::lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::PLUS, 1, 2, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::MINUS, 3, 4, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::PERCENT, 4, 5, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::AMPERSAND, 6, 7, ""));

    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::ASTERISK, 9, 10, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::DOT, 10, 11, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::SLASH, 11, 12, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::COLON, 12, 13, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::SEMICOLON, 13, 14, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::LESS, 14, 15, ""));

	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::EQUAL, 16, 17, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::GREATER, 18, 19, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::CARET, 21, 22, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::PIPE, 22, 23, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::TILDE, 24, 25, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::COMMA, 25, 26, ""));
}

TEST(Scanner, CompoundPunctuation) {
	auto source = std::istringstream("&& || >= <= != ... .. . += ++ -= -- *= /= << >> ==");
	argon::lang::scanner::Scanner scanner(&source);
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::AND, 1, 3, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::OR, 4, 6, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::GREATER_EQ, 7, 9, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::LESS_EQ, 10, 12, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NOT_EQUAL, 13, 15, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::ELLIPSIS, 16, 19, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::DOT, 20, 21, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::DOT, 21, 22, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::DOT, 23, 24, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::PLUS_EQ, 25, 27, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::PLUS_PLUS, 28, 30, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::MINUS_EQ, 31, 33, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::MINUS_MINUS, 34, 36, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::ASTERISK_EQ, 37, 39, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::SLASH_EQ, 40, 42, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::SHL, 43, 45, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::SHR, 46, 48, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::EQUAL_EQUAL, 49, 51, ""));
}

TEST(Scanner, String) {
    auto source = std::istringstream(R"("" "simple string" "\\" "Hello\"escaped\"")");
    argon::lang::scanner::Scanner scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 1, 3, ""));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 4, 19, "simple string"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 20, 24, "\\"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 25, 43, "Hello\"escaped\""));
}

TEST(Scanner, StringEscape) {
    auto source = std::istringstream(
            R"("bell\a" "\x7b" "\0" "\1" "\41" "\234" "\u0024" "\u03a3" "\u0939" "\U00010348" "Ignore\\"")");
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    argon::lang::scanner::Scanner scanner(&source);

    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 1, 9, "bell\a"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 10, 16, "{"));
    ASSERT_EQ(scanner.Next().value[0], 0x00);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 22, 26, "\1"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 27, 32, "\41"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 33, 39, "\234"));

    // Unicode UTF-8
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 40, 48, "\x24"));
    ASSERT_EQ(scanner.Next(),
              argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 49, 57, converter.to_bytes(L"\u03a3")));
    ASSERT_EQ(scanner.Next(),
              argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 58, 66, converter.to_bytes(L"\u0939")));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 67, 79, "\xF0\x90\x8D\x88"));

    // Ignore Escape
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::STRING, 80, 90, "Ignore\\"));
}

TEST(Scanner, UnterminatedString) {
	auto source = std::istringstream("\"hello");
	auto scanner = argon::lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR);

	source = std::istringstream("\"hello worl\nd\"");
	scanner = argon::lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR);
}

TEST(Scanner, bString) {
    auto source = std::istringstream(R"(b"ByteString" b"Ignore\u2342Unico\U00002312de" b"�")");
    auto scanner = argon::lang::scanner::Scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::BYTE_STRING, 1, 14, "ByteString"));
    ASSERT_EQ(scanner.Next(),
              argon::lang::scanner::Token(argon::lang::scanner::TokenType::BYTE_STRING, 15, 47, "Ignore\\u2342Unico\\U00002312de"));
    ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR); // Extended ASCII not allowed here!
}

TEST(Scanner, rString) {
    auto source = std::istringstream(R"(r"plain" r#"plain hash"# r###"multiple hash"### r##"internal " h"#sh#"## r####"New "###
Line!

rString"#### r"")");
    auto scanner = argon::lang::scanner::Scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RAW_STRING, 1, 9, "plain"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RAW_STRING, 10, 25, "plain hash"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RAW_STRING, 26, 48, "multiple hash"));
    ASSERT_EQ(scanner.Next(),
              argon::lang::scanner::Token(argon::lang::scanner::TokenType::RAW_STRING, 49, 73, "internal \" h\"#sh#"));
    ASSERT_EQ(scanner.Next(),
              argon::lang::scanner::Token(argon::lang::scanner::TokenType::RAW_STRING, 74, 108, "New \"###\nLine!\n\nrString"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::RAW_STRING, 109, 112, ""));

    source = std::istringstream("r\"Error!");
    scanner = argon::lang::scanner::Scanner(&source);
    ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR);

    source = std::istringstream("r#\"Error!\"");
    scanner = argon::lang::scanner::Scanner(&source);
    ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR);

    source = std::istringstream("r##\"Error!##");
	scanner = argon::lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR);

	source = std::istringstream("r##Error!\"##");
	scanner = argon::lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.Next().type, argon::lang::scanner::TokenType::ERROR);
}

TEST(Scanner, Comments) {
	auto source = std::istringstream(R"(var_name # inline comment
/*
Multi
* /* *\/
line comment
291019G.<3
*/)");
	auto scanner = argon::lang::scanner::Scanner(&source);
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::IDENTIFIER, 1, 9, "var_name"));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::INLINE_COMMENT, 10, 27, "inline comment"));
    ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::COMMENT, 27, 71, "Multi\n* /* *\\/\nline comment\n291019G.<3\n"));
}

TEST(Scanner, Peek) {
	auto source = std::istringstream(R"(1+2)");
	auto scanner = argon::lang::scanner::Scanner(&source);
	ASSERT_EQ(scanner.Peek(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 1, 2, "1"));
	ASSERT_EQ(scanner.Peek(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 1, 2, "1"));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 1,2, "1"));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::PLUS, 2,3, ""));
	ASSERT_EQ(scanner.Peek(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 3, 4, "2"));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::NUMBER, 3, 4, "2"));
	ASSERT_EQ(scanner.Peek(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_FILE, 4,4, ""));
	ASSERT_EQ(scanner.Next(), argon::lang::scanner::Token(argon::lang::scanner::TokenType::END_OF_FILE, 4, 4, ""));
}
