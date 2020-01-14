// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <lang/parser.h>
#include <lang/syntax_exception.h>

using namespace lang;
using namespace lang::ast;

/*
TEST(Parser, ImplDecl) {
    auto source = std::istringstream("impl core::test for xyz {}");
    Parser parser(&source);

    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::IMPL);

    source = std::istringstream("impl core::test for xyz [ }");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Relational) {
    auto source = std::istringstream("mystruct.item * 0 >= 0");
    Parser parser(&source);
    auto ast = parser.Parse();
    ASSERT_EQ(ast->stmts.front()->type, NodeType::RELATIONAL);
    ASSERT_EQ(CastNode<Binary>(ast->stmts.front())->kind, lang::scanner::TokenType::GREATER_EQ);
}

TEST(Parser, MulExpr) {
    auto source = std::istringstream("mystruct.item * 24");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MUL);
}

TEST(Parser, Unary) {
    auto source = std::istringstream("-mystruct.item");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MINUS);
}

TEST(Parser, MemberAccess) {
    auto source = std::istringstream("mymstruct.item");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MEMBER);

    source = std::istringstream("mystruct?.item");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MEMBER_SAFE);

    source = std::istringstream("mystruct!.item");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MEMBER_ASSERT);

    source = std::istringstream("mystruct?.");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, List) {
    auto source = std::istringstream("[1,2]");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::LIST);
}

TEST(Parser, Map) {
    auto source = std::istringstream("{}");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MAP);

    source = std::istringstream(R"({"key":"value"})");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::MAP);

    source = std::istringstream(R"({"key":})");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Set) {
    auto source = std::istringstream(R"({"value"})");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::SET);

    source = std::istringstream(R"({"value", "value1", 23, []})");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->stmts.front()->type, NodeType::SET);

    source = std::istringstream(R"({"key",})");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}
 */

TEST(Parser, List) {
    auto source = std::istringstream("[]");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::LIST);

    source = std::istringstream("[1,2,3]");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::LIST);

    source = std::istringstream("[1,]");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("[1");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Literals) {
    auto source = std::istringstream("2");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::LITERAL);

    source = std::istringstream("24.06");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::LITERAL);

    source = std::istringstream("r##\"raw string\"##");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::LITERAL);
}

TEST(Parser, IdentifierAndScope) {
    auto source = std::istringstream("identifier");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::IDENTIFIER);

    source = std::istringstream("identifier::identifier1::id2");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SCOPE);

    source = std::istringstream("identifier::");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("identifier::12");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}