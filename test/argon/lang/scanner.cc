// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <lang/scanner/scanner.h>
#include <lang/scanner/token.h>

using namespace argon::lang::scanner;

void TokenEq(Token &tk, TokenType type, Pos start, Pos end, const char *value) {
    ASSERT_EQ(tk.type, type);
    ASSERT_EQ(tk.start, start);
    ASSERT_EQ(tk.end, end);
    ASSERT_STREQ(reinterpret_cast<const char *>(tk.buf), value);
}

TEST(Scanner, EmptyInput) {
    Scanner scanner("");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 1, 1, nullptr);
}

TEST(Scanner, NewLine) {
    Scanner scanner("\n\n");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_LINE, 1, 3, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 3, 3, nullptr);
}

TEST(Scanner, Spaces) {
    Scanner scanner("\n   \n\n \n");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_LINE, 1, 2, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_LINE, 5, 7, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_LINE, 8, 9, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 9, 9, nullptr);
}

TEST(Scanner, BinaryNumber) {
    Scanner scanner("0b1010 0B101");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER_BIN, 1, 7, "1010");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER_BIN, 8, 13, "101");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 13, 13, nullptr);
}

TEST(Scanner, BinaryOctal) {
    Scanner scanner("0o23423 0O02372");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER_OCT, 1, 8, "23423");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER_OCT, 9, 16, "02372");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 16, 16, nullptr);
}

TEST(Scanner, BinaryHex) {
    Scanner scanner("0xaba12 0X19Fa");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER_HEX, 1, 8, "aba12");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER_HEX, 9, 15, "19Fa");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 15, 15, nullptr);
}

TEST(Scanner, Numbers) {
    Scanner scanner("123 010697 1 0 22.3 6. 0.");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER, 1, 4, "123");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER, 5, 11, "010697");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER, 12, 13, "1");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NUMBER, 14, 15, "0");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::DECIMAL, 16, 20, "22.3");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::DECIMAL, 21, 23, "6.");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::DECIMAL, 24, 26, "0.");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 26, 26, nullptr);
}

TEST(Scanner, Word) {
    Scanner scanner("vax v4r v_48_ __private_var__ byte b else if");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 1, 4, "vax");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 5, 8, "v4r");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 9, 14, "v_48_");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 15, 30, "__private_var__");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 31, 35, "byte");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 36, 37, "b");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ELSE, 38, 42, "else");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::IF, 43, 45, "if");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 45, 45, nullptr);
}

TEST(Scanner, Delimiters) {
    Scanner scanner("() ][ {}");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::LEFT_ROUND, 1, 2, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RIGHT_ROUND, 2, 3, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RIGHT_SQUARE, 4, 5, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::LEFT_SQUARE, 5,6, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::LEFT_BRACES, 7,8, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RIGHT_BRACES, 8, 9, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 9, 9, nullptr);
}

TEST(Scanner, Punctuation) {
    Scanner scanner("+ -% &  *./:;< = >  ^| ~,");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::PLUS, 1, 2, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::MINUS, 3, 4, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::PERCENT, 4, 5, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::AMPERSAND, 6,7, nullptr);

    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ASTERISK, 9,10, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::DOT, 10, 11, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::SLASH, 11,12, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::COLON, 12, 13, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::SEMICOLON, 13,14, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::LESS, 14, 15, nullptr);

    tk = scanner.NextToken();
    TokenEq(tk, TokenType::EQUAL, 16,17, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::GREATER, 18, 19, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::CARET, 21,22, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::PIPE, 22, 23, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::TILDE, 24,25, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::COMMA, 25, 26, nullptr);

    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 26, 26, nullptr);
}

TEST(Scanner, CompoundPunctuation) {
    Scanner scanner("&& || >= <= != ... .. . += ++ -= -- *= /= << >> ==");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::AND, 1, 3, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::OR, 4, 6, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::GREATER_EQ, 7, 9, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::LESS_EQ, 10, 12, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::NOT_EQUAL, 13, 15, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ELLIPSIS, 16, 19, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 20, 23, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::DOT, 23, 24, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::PLUS_EQ, 25, 27, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::PLUS_PLUS, 28, 30, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::MINUS_EQ, 31, 33, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::MINUS_MINUS, 34, 36, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ASTERISK_EQ, 37, 39, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::SLASH_EQ, 40, 42, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::SHL, 43, 45, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::SHR, 46, 48, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::EQUAL_EQUAL, 49, 51, nullptr);
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 51, 51, nullptr);
}

TEST(Scanner, String) {
    Scanner scanner(R"("" "simple string" "\\" "Hello\"escaped\"")");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 1, 3, "");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 4, 19, "simple string");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 20, 24, "\\");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 25, 43, "Hello\"escaped\"");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 43, 43, nullptr);
}

TEST(Scanner, StringEscape) {
    Scanner scanner(R"("bell\a" "\x7b" "\0" "\1" "\41" "\234" "\u0024" "\u03a3" "\u0939" "\U00010348" "Ignore\\")");
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 1, 9, "bell\a");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 10, 16, "{");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 17, 21, "\0");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 22, 26, "\1");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 27, 32, "\41");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 33, 39, "\234");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 40, 48, "\x24");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 49, 57, converter.to_bytes(L"\u03a3").c_str());
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 58, 66, converter.to_bytes(L"\u0939").c_str());
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 67, 79, "\xF0\x90\x8D\x88");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::STRING, 80, 90, "Ignore\\");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 90, 90, nullptr);
}

TEST(Scanner, UnterminatedString) {
    Scanner scanner("\"hello");

    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 1, 7, nullptr);

    scanner = Scanner("\"hello worl\nd\"");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 1, 13, nullptr);
}

TEST(Scanner, bString) {
    Scanner scanner(R"(b"ByteString" b"Ignore\u2342Unico\U00002312de" b"�")");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::BYTE_STRING, 1, 14, "ByteString");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::BYTE_STRING, 15, 47, "Ignore\\u2342Unico\\U00002312de");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 48, 51, nullptr);
}

TEST(Scanner, rString) {
    Scanner scanner(R"(r"plain" r#"plain hash"# r###"multiple hash"### r##"internal " h"#sh#"## r####"New "###
Line!

rString"#### r"")");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 1, 9, "plain");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 10, 25, "plain hash");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 26, 48, "multiple hash");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 49, 73, "internal \" h\"#sh#");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 74, 108, "New \"###\nLine!\n\nrString");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 109, 112, "");

    scanner = Scanner("r\"ok!\"");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::RAW_STRING, 1, 7, "ok!");

    scanner = Scanner("r\"Error!");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 1, 9, nullptr);

    scanner = Scanner("r#\"Error!\"");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 1, 11, nullptr);

    scanner = Scanner("r##\"Error!##");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 1, 13, nullptr);

    scanner = Scanner("r##Error!\"##");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::ERROR, 1, 5, nullptr);
}

TEST(Scanner, Comments) {
    Scanner scanner(R"(var_name # inline comment ?
/*
Multi
* /* *\/
line comment
291019G.<3
*/)");
    Token tk = scanner.NextToken();
    TokenEq(tk, TokenType::IDENTIFIER, 1, 9, "var_name");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::INLINE_COMMENT, 10, 29, "inline comment ?");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::COMMENT, 29, 73, "Multi\n* /* *\\/\nline comment\n291019G.<3\n");
    tk = scanner.NextToken();
    TokenEq(tk, TokenType::END_OF_FILE, 73, 73, nullptr);
}

/*
TEST(Scanner, Peek) {
    Scanner scanner(R"(1+2)");

    Token tk = scanner.NextToken();
    //TokenEq(tk, TokenType::ERROR, 1, 7, nullptr);
}
 */