// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <lang/ast/ast.h>
#include "parser.h"
#include "syntax_exception.h"

using namespace lang;
using namespace lang::ast;
using namespace lang::scanner;

Parser::Parser(std::string filename, std::istream *source) {
    this->scanner_ = std::make_unique<Scanner>(source);
    this->currTk_ = this->scanner_->Next();
}

void Parser::Eat() {
    this->currTk_ = this->scanner_->Next();
}

void Parser::Eat(TokenType type, std::string errmsg) {
    if (!this->Match(type))
        throw SyntaxException(std::move(errmsg), this->currTk_);
    this->currTk_ = this->scanner_->Next();
}

ast::NodeUptr Parser::Expression() {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    NodeUptr left = this->TestList();

    if (this->Match(TokenType::EQUAL)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::ASSIGN,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->TestList(),
                                        colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::TestList() {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    NodeUptr left = this->Test();

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        if (left->type != NodeType::TUPLE) {
            auto tuple = std::make_unique<List>(NodeType::TUPLE, colno, lineno);
            tuple->AddExpression(std::move(left));
            left = std::move(tuple);
        }
        CastNode<List>(left)->AddExpression(this->Test());
    }

    return left;
}

ast::NodeUptr Parser::Test() {
    NodeUptr left = this->OrTest();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::ELVIS)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::ELVIS, std::move(left), this->TestList(), colno, lineno);
    } else if (this->Match(TokenType::QUESTION)) {
        this->Eat();
        // TODO after IF Node
    }

    return left;
}

ast::NodeUptr Parser::OrTest() {
    NodeUptr left = this->AndTest();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::OR)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::OR_TEST, std::move(left), this->OrTest(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::AndTest() {
    NodeUptr left = this->OrExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::AND)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::AND_TEST, std::move(left), this->AndTest(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::OrExpr() {
    NodeUptr left = this->XorExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::PIPE)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::LOGICAL_OR, std::move(left), this->OrExpr(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::XorExpr() {
    NodeUptr left = this->AndExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::CARET)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::LOGICAL_XOR, std::move(left), this->XorExpr(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::AndExpr() {
    NodeUptr left = this->EqualityExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::AMPERSAND)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::LOGICAL_AND, std::move(left), this->AndExpr(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::EqualityExpr() {
    NodeUptr left = this->RelationalExpr();
    TokenType kind = this->currTk_.type;
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::EQUAL_EQUAL, TokenType::NOT_EQUAL)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::EQUALITY, kind, std::move(left), this->RelationalExpr(), colno,
                                        lineno);
    }

    return left;
}

ast::NodeUptr Parser::RelationalExpr() {
    NodeUptr left = this->ShiftExpr();
    TokenType kind = this->currTk_.type;
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->TokenInRange(TokenType::RELATIONAL_BEGIN, TokenType::RELATIONAL_END)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::RELATIONAL, kind, std::move(left), this->ShiftExpr(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::ShiftExpr() {
    NodeUptr left = this->ArithExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->currTk_.type == TokenType::SHL) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SHL, std::move(left), this->ShiftExpr(), colno, lineno);
    }

    if (this->currTk_.type == TokenType::SHR) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SHR, std::move(left), this->ShiftExpr(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::ArithExpr() {
    NodeUptr left = this->MulExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->currTk_.type == TokenType::PLUS) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SUM, std::move(left), this->ArithExpr(), colno, lineno);
    }

    if (this->currTk_.type == TokenType::MINUS) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SUB, std::move(left), this->ArithExpr(), colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::MulExpr() {
    NodeUptr left = this->UnaryExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    switch (this->currTk_.type) {
        case TokenType::ASTERISK:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MUL, std::move(left), this->MulExpr(), colno, lineno);
        case TokenType::SLASH:
            this->Eat();
            return std::make_unique<Binary>(NodeType::DIV, std::move(left), this->MulExpr(), colno, lineno);
        case TokenType::SLASH_SLASH:
            this->Eat();
            return std::make_unique<Binary>(NodeType::INTEGER_DIV, std::move(left), this->MulExpr(), colno, lineno);
        case TokenType::PERCENT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::REMINDER, std::move(left), this->MulExpr(), colno, lineno);
        default:
            return left;
    }
}

ast::NodeUptr Parser::UnaryExpr() {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    switch (this->currTk_.type) {
        case TokenType::EXCLAMATION:
            this->Eat();
            return std::make_unique<Unary>(NodeType::NOT, this->UnaryExpr(), colno, lineno);
        case TokenType::TILDE:
            this->Eat();
            return std::make_unique<Unary>(NodeType::BITWISE_NOT, this->UnaryExpr(), colno, lineno);
        case TokenType::PLUS:
            this->Eat();
            return std::make_unique<Unary>(NodeType::PLUS, this->UnaryExpr(), colno, lineno);
        case TokenType::MINUS:
            this->Eat();
            return std::make_unique<Unary>(NodeType::MINUS, this->UnaryExpr(), colno, lineno);
        case TokenType::PLUS_PLUS:
            this->Eat();
            return std::make_unique<Unary>(NodeType::PREFIX_INC, this->UnaryExpr(), colno, lineno);
        case TokenType::MINUS_MINUS:
            this->Eat();
            return std::make_unique<Unary>(NodeType::PREFIX_DEC, this->UnaryExpr(), colno, lineno);
        default:
            return this->AtomExpr();
    }
}

ast::NodeUptr Parser::AtomExpr() {
    auto left = this->ParseAtom();

    while (this->Trailer(left));

    return left;
}

bool Parser::Trailer(NodeUptr &left) {
    switch (this->currTk_.type) {
        case TokenType::LEFT_ROUND:
            // TODO: arguments
        case TokenType::LEFT_SQUARE:
            // TODO: slice
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
        case TokenType::EXCLAMATION_DOT:
            left = this->MemberAccess(std::move(left));
            return true;
        case TokenType::PLUS_PLUS:
            this->Eat();
            left = std::make_unique<Unary>(NodeType::POSTFIX_INC, std::move(left), this->currTk_.colno,
                                           this->currTk_.lineno);
            return true;
        case TokenType::MINUS_MINUS:
            this->Eat();
            left = std::make_unique<Unary>(NodeType::POSTFIX_DEC, std::move(left), this->currTk_.colno,
                                           this->currTk_.lineno);
            return true;
        default:
            return false;
    }
}

ast::NodeUptr Parser::MemberAccess(ast::NodeUptr left) {
    switch (this->currTk_.type) {
        case TokenType::DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER, std::move(left), this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
        case TokenType::QUESTION_DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_SAFE, std::move(left), this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
        default:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_ASSERT, std::move(left), this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
    }
}

ast::NodeUptr Parser::ParseAtom() {
    // TODO: <arrow>
    NodeUptr tmp;

    switch (this->currTk_.type) {
        case TokenType::FALSE:
        case TokenType::TRUE:
        case TokenType::NIL:
            return std::make_unique<ast::Literal>(this->currTk_);
        case TokenType::LEFT_ROUND:
            this->Eat();
            tmp = this->TestList();
            this->Eat(TokenType::RIGHT_ROUND, "expected ) after parenthesized expression");
            return tmp;
        case TokenType::LEFT_SQUARE:
            return this->ParseList();
        case TokenType::LEFT_BRACES:
            return this->ParseMapOrSet();
        default:
            if ((tmp = this->ParseNumber()))
                return tmp;

            if ((tmp = this->ParseString()))
                return tmp;
    }

    return this->ParseScope();
    //throw SyntaxException("invalid syntax", this->currTk_);
}

ast::NodeUptr Parser::ParseList() {
    NodeUptr list = std::make_unique<List>(NodeType::LIST, this->currTk_.colno, this->currTk_.lineno);

    this->Eat();

    if (!this->Match(TokenType::RIGHT_SQUARE))
        CastNode<List>(list)->AddExpression(this->Test());

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        CastNode<List>(list)->AddExpression(this->Test());
    }

    this->Eat(TokenType::RIGHT_SQUARE, "expected ] after list definition");

    return list;
}

ast::NodeUptr Parser::ParseMapOrSet() {
    NodeUptr ms = std::make_unique<List>(NodeType::MAP, this->currTk_.colno, this->currTk_.lineno);

    this->Eat();

    if (this->Match(TokenType::RIGHT_BRACES)) {
        this->Eat();
        return ms;
    }

    CastNode<List>(ms)->AddExpression(this->Test());

    if (this->Match(TokenType::COLON))
        this->ParseMap(ms);
    else if (this->Match(TokenType::COMMA)) {
        ((Node *) ms.get())->type = NodeType::SET;
        do {
            this->Eat();
            CastNode<List>(ms)->AddExpression(this->Test());
        } while (this->Match(TokenType::COMMA));
    }

    if (CastNode<List>(ms)->expressions.size() == 1)
        ((Node *) ms.get())->type = NodeType::SET;

    this->Eat(TokenType::RIGHT_BRACES, "expected } after map/set definition");
    return ms;
}

void Parser::ParseMap(ast::NodeUptr &node) {
    bool mustContinue;
    do {
        mustContinue = false;
        this->Eat(TokenType::COLON, "missing :value after key in the map definition");
        CastNode<List>(node)->AddExpression(this->Test());
        if (!this->Match(TokenType::COMMA))
            break;
        this->Eat();
        CastNode<List>(node)->AddExpression(this->Test());
        mustContinue = true;
    } while (mustContinue);
}

ast::NodeUptr Parser::ParseNumber() {
    NodeUptr tmp;
    if (this->TokenInRange(TokenType::NUMBER_BEGIN, TokenType::NUMBER_END)) {
        tmp = std::make_unique<ast::Literal>(this->currTk_);
        this->Eat();
    }
    return tmp;
}

ast::NodeUptr Parser::ParseString() {
    NodeUptr tmp;
    if (this->TokenInRange(TokenType::STRING_BEGIN, TokenType::STRING_END)) {
        tmp = std::make_unique<ast::Literal>(this->currTk_);
        this->Eat();
    }
    return tmp;
}

ast::NodeUptr Parser::ParseScope() {
    NodeUptr scope;
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::IDENTIFIER)) {
        scope = std::make_unique<Literal>(this->currTk_);
        this->Eat();

        if (this->Match(TokenType::SCOPE)) {
            auto tmp = std::make_unique<Scope>(colno, lineno);
            tmp->AddSegment(CastNode<Literal>(scope)->value);
            do {
                this->Eat();
                if (!this->Match(TokenType::IDENTIFIER))
                    throw SyntaxException("expected identifier after ::(scope resolution)", this->currTk_);
                tmp->AddSegment(this->currTk_.value);
                this->Eat();
            } while (this->Match(TokenType::SCOPE));
            scope = std::move(tmp);
        }
        return scope;
    }

    throw SyntaxException("expected identifier or expression", this->currTk_);
}

ast::NodeUptr Parser::Parse() {
    return this->Expression();
}
