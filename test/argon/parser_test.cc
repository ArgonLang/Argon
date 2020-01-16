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
 */

TEST(Parser, Assign) {
    auto source = std::istringstream("var1=a+b");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ASSIGN);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 9);

    source = std::istringstream("var1 = a?.test ?: 24");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ASSIGN);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 21);
}

TEST(Parser, TestList) {
    auto source = std::istringstream("(alfa.beta ?: ab-2, 2), out");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TUPLE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 28);

    source = std::istringstream("a?c,");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, ElvisTernaryTest) {
    auto source = std::istringstream("alfa.beta ?: ab-2");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ELVIS);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 18);

    source = std::istringstream("a>2||c?val1:val2");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IF);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 17);

    source = std::istringstream("a?c");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("a?c:");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, AndOrTest) {
    auto source = std::istringstream("a&b^c&&a");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TEST);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::AND);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 9);

    source = std::istringstream("b&&c||true");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TEST);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::OR);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 11);
}

TEST(Parser, AndOrXorExpr) {
    auto source = std::istringstream("a&b^c");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::LOGICAL);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::CARET);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 6);

    source = std::istringstream("a&b&c");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::LOGICAL);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::AMPERSAND);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 6);

    source = std::istringstream("c^a&b^c|a");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::LOGICAL);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::PIPE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 10);
}

TEST(Parser, Equality) {
    auto source = std::istringstream("a+b>4==true");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::EQUALITY);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::EQUAL_EQUAL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 12);

    source = std::istringstream("a+b!=4>6");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::EQUALITY);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::NOT_EQUAL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 9);
}

TEST(Parser, Relational) {
    auto source = std::istringstream("a+b>4");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::RELATIONAL);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::GREATER);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 6);

    source = std::istringstream("a<=b+2");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::RELATIONAL);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::LESS_EQ);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 7);
}

TEST(Parser, ShiftExpr) {
    auto source = std::istringstream("a>>b");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::BINARY_OP);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::SHR);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 5);

    source = std::istringstream("a<<b");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::BINARY_OP);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::SHL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 5);
}

TEST(Parser, MulSumExpr) {
    auto source = std::istringstream("a+b");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::BINARY_OP);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::PLUS);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 4);

    source = std::istringstream("a.x*22");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::BINARY_OP);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::ASTERISK);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 7);

    source = std::istringstream("a.x*22//44");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::BINARY_OP);
    ASSERT_EQ(CastNode<Binary>(tmp.front())->kind, scanner::TokenType::ASTERISK);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 11);
}

TEST(Parser, Unary) {
    auto source = std::istringstream("++a");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::UPDATE);
    ASSERT_EQ(CastNode<Update>(tmp.front())->kind, scanner::TokenType::PLUS_PLUS);
    ASSERT_TRUE(CastNode<Update>(tmp.front())->prefix);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 4);

    source = std::istringstream("--mystruct?.item");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::UPDATE);
    ASSERT_EQ(CastNode<Update>(tmp.front())->kind, scanner::TokenType::MINUS_MINUS);
    ASSERT_TRUE(CastNode<Update>(tmp.front())->prefix);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 17);

    source = std::istringstream("~mystruct?.item");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::UNARY_OP);
    ASSERT_EQ(CastNode<Unary>(tmp.front())->kind, scanner::TokenType::TILDE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 16);
}

TEST(Parser, PostfixUpdate) {
    auto source = std::istringstream("mymstruct.item++");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::UPDATE);
    ASSERT_EQ(CastNode<Update>(tmp.front())->kind, scanner::TokenType::PLUS_PLUS);
    ASSERT_FALSE(CastNode<Update>(tmp.front())->prefix);

    source = std::istringstream("mystruct?.item--");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::UPDATE);
    ASSERT_EQ(CastNode<Update>(tmp.front())->kind, scanner::TokenType::MINUS_MINUS);
    ASSERT_FALSE(CastNode<Update>(tmp.front())->prefix);
}

TEST(Parser, MemberAccess) {
    auto source = std::istringstream("mymstruct.item");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MEMBER);

    source = std::istringstream("mystruct?.item");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MEMBER_SAFE);

    source = std::istringstream("mystruct!.item");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MEMBER_ASSERT);

    source = std::istringstream("mystruct!.item.subitem");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MEMBER);

    source = std::istringstream("mystruct?.");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Subscript) {
    auto source = std::istringstream("[[0,1,2],2,3][0][1]");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SUBSCRIPT);

    source = std::istringstream("[1,2,3][a:b]");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SUBSCRIPT);

    source = std::istringstream("[1,2,3][a:b+1:2]");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SUBSCRIPT);

    source = std::istringstream("[1,2,3][1:]");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("[1,2,3][1:2:]");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("[1,2,3][1::]");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, FnCall) {
    auto source = std::istringstream("call()");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::CALL);

    source = std::istringstream("call(1,2,3)");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::CALL);

    source = std::istringstream("call(a...)");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::CALL);

    source = std::istringstream("call(a+b,c...)");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::CALL);

    source = std::istringstream("[(a,b,c)=>{}][0](1,2,3)");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::CALL);

    source = std::istringstream("call(a,b,c...,d)");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("call(a...,b)");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("call(a,)");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, ArrowFn) {
    auto source = std::istringstream("()=>{}");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::FUNC);

    source = std::istringstream("(a,b,c)=>{}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::FUNC);

    source = std::istringstream("(a,b,c,...d)=>{}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::FUNC);

    source = std::istringstream("(...a)=>{}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::FUNC);

    source = std::istringstream("(a,b,2)=>{}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("(a,b,...c,d)=>{}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("(...c,d)=>{}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Tuple) {
    auto source = std::istringstream("()");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::TUPLE);

    source = std::istringstream("(a,b)");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::TUPLE);

    source = std::istringstream("(a,b+2,3)");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::TUPLE);

    source = std::istringstream("(a,)");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("(a,...b)");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, MapSet) {
    auto source = std::istringstream("{}");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MAP);

    source = std::istringstream("{key:01}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MAP);

    source = std::istringstream("{key:24, keyb:06}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MAP);

    source = std::istringstream("{22}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SET);

    source = std::istringstream("{01,24,06,94}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SET);

    source = std::istringstream("{1");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("{keya:}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("{keya:keyb,keyc}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

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