// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <lang/parser.h>
#include <lang/syntax_exception.h>

using namespace lang;
using namespace lang::ast;

TEST(Parser, Alias) {
    auto source = std::istringstream("using id as identifier");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ALIAS);
    ASSERT_FALSE(CastNode<Alias>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 23);

    source = std::istringstream("pub using id as identifier");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ALIAS);
    ASSERT_TRUE(CastNode<Alias>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 27);

    source = std::istringstream("pub using id as id::e::nt");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ALIAS);
    ASSERT_TRUE(CastNode<Alias>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 26);

    source = std::istringstream("using ident");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("using ident as ");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Variable) {
    auto source = std::istringstream("var x");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::VARIABLE);
    ASSERT_FALSE(CastNode<Variable>(tmp.front())->weak);
    ASSERT_FALSE(CastNode<Variable>(tmp.front())->atomic);
    ASSERT_FALSE(CastNode<Variable>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 6);

    source = std::istringstream("var x: scope1::scope2");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::VARIABLE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 22);

    source = std::istringstream("var x: scope = a+b");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::VARIABLE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 19);

    source = std::istringstream("var x = a+b");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::VARIABLE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 12);

    source = std::istringstream("pub atomic weak var paw= obj");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::VARIABLE);
    ASSERT_TRUE(CastNode<Variable>(tmp.front())->weak);
    ASSERT_TRUE(CastNode<Variable>(tmp.front())->atomic);
    ASSERT_TRUE(CastNode<Variable>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 29);

    source = std::istringstream("weak var paw= obj");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::VARIABLE);
    ASSERT_TRUE(CastNode<Variable>(tmp.front())->weak);
    ASSERT_FALSE(CastNode<Variable>(tmp.front())->atomic);
    ASSERT_FALSE(CastNode<Variable>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 18);
}

TEST(Parser, Constant) {
    auto source = std::istringstream("let x = obj");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::CONSTANT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 12);

    source = std::istringstream("pub let x=abc");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::CONSTANT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 14);

    source = std::istringstream("pub weak let x = obj");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("let wrong");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("let wrong =");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Function) {
    auto source = std::istringstream("func function{}");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FUNC);
    ASSERT_FALSE(CastNode<Function>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 16);

    source = std::istringstream("func function() {}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FUNC);
    ASSERT_FALSE(CastNode<Function>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 19);

    source = std::istringstream("func function(param1, param2, ...params) {}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FUNC);
    ASSERT_FALSE(CastNode<Function>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 44);

    source = std::istringstream("pub func function(a) {}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FUNC);
    ASSERT_TRUE(CastNode<Function>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 24);
}

TEST(Parser, Struct) {
    auto source = std::istringstream(R"(struct Test {})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT);
    ASSERT_FALSE(CastNode<Construct>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 15);

    source = std::istringstream(R"(pub struct Test impl a,b,c::d {
})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT);
    ASSERT_TRUE(CastNode<Construct>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 34);

    source = std::istringstream(R"(pub struct Test {
var v1
func String{}
})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT);
    ASSERT_TRUE(CastNode<Construct>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 41);

    source = std::istringstream("struct Test {");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("struct Test { let v1 }");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Trait) {
    auto source = std::istringstream(R"(trait Test {})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TRAIT);
    ASSERT_FALSE(CastNode<Construct>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 14);

    source = std::istringstream(R"(pub trait Test : t1,t2 {})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TRAIT);
    ASSERT_TRUE(CastNode<Construct>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 26);

    source = std::istringstream(R"(pub trait Test {
func String{}
})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TRAIT);
    ASSERT_TRUE(CastNode<Construct>(tmp.front())->pub);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 33);

    source = std::istringstream("trait Test {");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("trait Test { var v1 }");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Impl) {
    auto source = std::istringstream(R"(impl Test {})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IMPL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 13);

    source = std::istringstream(R"(impl string::Stringer for Test {})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IMPL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 34);

    source = std::istringstream("impl Test { var error }");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, JmpLabel) {
    auto source = std::istringstream(R"(label:
if a+b < 5 {
a++
goto label
})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::LABEL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 37);

    source = std::istringstream("label:");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Import) {
    auto source = std::istringstream("import regex as re, system as sys");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IMPORT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 34);

    source = std::istringstream("import sys::io");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IMPORT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 15);

    source = std::istringstream("from a::b::c import fn1 as a, f2, fn3 as func3");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IMPORT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 47);

    source = std::istringstream("from a::b import suba::sub");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("import a::b as ab::ba");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}


TEST(Parser, ForStmt) {
    auto source = std::istringstream(R"(for ;i<10;i++{})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FOR);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 16);

    source = std::istringstream(R"(for var i=0;i<5;i++ { j=j*i })");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FOR);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 30);

    source = std::istringstream(R"(for i in "string" {
print(i)})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FOR_IN);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 30);

    source = std::istringstream(R"(for k,v in mydict {print(k,v)})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::FOR_IN);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 31);

    source = std::istringstream(R"(for ;i<10; {})");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream(R"(for a,b,a+2 in object {})");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, LoopStmt) {
    auto source = std::istringstream(R"(loop{
counter ++})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::LOOP);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 18);

    source = std::istringstream(R"(loop counter <= 100 { counter++ })");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::LOOP);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 34);
}

TEST(Parser, IfStmt) {
    auto source = std::istringstream(R"(if x {return a})");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IF);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 16);

    source = std::istringstream(R"(if x {return a} else {return b})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IF);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 32);

    source = std::istringstream(R"(if a>b {return a} elif a<b {return b} elif a==b {return a+b})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IF);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 61);

    source = std::istringstream(R"(if a>b {return a} elif a<b {return b} elif a==b {return a+b} else {return 0})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::IF);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 77);
}

TEST(Parser, Switch) {
    auto source = std::istringstream(R"(switch test {case a: case b: b-- })");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::SWITCH);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 35);

    source = std::istringstream(R"(switch {
case a: x+y})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::SWITCH);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 22);

    source = std::istringstream(R"(switch {case a | b | c+3: return f case z: return a+b})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::SWITCH);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 55);

    source = std::istringstream(R"(switch {
default: x+y
case a:
default:
})");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, Assign) {
    auto source = std::istringstream("var1=a+b");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ASSIGN);
    ASSERT_EQ(CastNode<Assignment>(tmp.front())->kind, scanner::TokenType::EQUAL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 9);

    source = std::istringstream("var1 = a?.test ?: 24");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ASSIGN);
    ASSERT_EQ(CastNode<Assignment>(tmp.front())->kind, scanner::TokenType::EQUAL);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 21);

    source = std::istringstream("var1 += a?.test ?: 24");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::ASSIGN);
    ASSERT_EQ(CastNode<Assignment>(tmp.front())->kind, scanner::TokenType::PLUS_EQ);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 22);
}

TEST(Parser, TestList) {
    auto source = std::istringstream("(alfa.beta ?: ab-2, 2), out");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TUPLE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 28);

    source = std::istringstream(R"((alfa+2,
beta
,
gamma))");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::TUPLE);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 23);

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

TEST(Parser, StructInit) {
    auto source = std::istringstream("alfa::beta!{}");
    Parser parser(&source);
    auto tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 14);

    source = std::istringstream("test!{22}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 10);

    source = std::istringstream("test!{one: value1, two: 2+2}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 29);

    source = std::istringstream(R"(test!{one:
value1
,
two
:
2+2})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 31);

    source = std::istringstream("test!{one: value1"
                                ",two: 2+2}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 28);

    source = std::istringstream("test!{1,2,3,element+2}");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 23);

    source = std::istringstream(R"(test !{
1,
2
,3
,element+2})");
    parser = Parser(&source);
    tmp = std::move(parser.Parse()->body);
    ASSERT_EQ(tmp.front()->type, NodeType::STRUCT_INIT);
    ASSERT_EQ(tmp.front()->start, 1);
    ASSERT_EQ(tmp.front()->end, 28);

    source = std::istringstream("test!{2+2:x}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);

    source = std::istringstream("test!{a:b,c:d,a[2]:x}");
    parser = Parser(&source);
    EXPECT_THROW(parser.Parse(), SyntaxException);
}

TEST(Parser, MemberAccess) {
    auto source = std::istringstream("mymstruct.item");
    Parser parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MEMBER);

    source = std::istringstream("mystruct?.item");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MEMBER_SAFE);

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

    source = std::istringstream(R"((
a
,
b,
c
)=>{})");
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

    source = std::istringstream(R"({
key
:
24
,
keyb:06
})");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::MAP);

    source = std::istringstream("{22}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SET);

    source = std::istringstream("{01,24,06,94}");
    parser = Parser(&source);
    ASSERT_EQ(parser.Parse()->body.front()->type, NodeType::SET);

    source = std::istringstream(R"({
01
,
24,
06, 94
})");
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

    source = std::istringstream(R"([
1
,
2,
3])");
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