// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "parser.h"

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
        return std::make_unique<Binary>(NodeType::ELVIS,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->TestList(),
                                        colno, lineno);
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
        return std::make_unique<Binary>(NodeType::OR_TEST,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->OrTest(),
                                        colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::AndTest() {
    NodeUptr left = this->OrExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::AND)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::AND_TEST,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->AndExpr(),
                                        colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::OrExpr() {
    NodeUptr left = this->XorExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::PIPE)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::LOGICAL_OR,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->OrExpr(),
                                        colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::XorExpr() {
    NodeUptr left = this->AndExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::CARET)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::LOGICAL_XOR,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->XorExpr(),
                                        colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::AndExpr() {
    NodeUptr left = this->EqualityExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->Match(TokenType::AMPERSAND)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::LOGICAL_AND,
                                        TokenType::TK_NULL,
                                        std::move(left),
                                        this->AndExpr(),
                                        colno, lineno);
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
        return std::make_unique<Binary>(NodeType::EQUALITY,
                                        kind,
                                        std::move(left),
                                        this->RelationalExpr(),
                                        colno, lineno);
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
        return std::make_unique<Binary>(NodeType::RELATIONAL,
                                        kind,
                                        std::move(left),
                                        this->ShiftExpr(),
                                        colno, lineno);
    }

    return left;
}

ast::NodeUptr Parser::ShiftExpr() {
    NodeUptr left = this->ArithExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->currTk_.type == TokenType::SHL) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SHL, TokenType::TK_NULL, std::move(left), this->ShiftExpr(), colno,
                                        lineno);
    }

    if (this->currTk_.type == TokenType::SHR) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SHR, TokenType::TK_NULL, std::move(left), this->ShiftExpr(), colno,
                                        lineno);
    }

    return left;
}

ast::NodeUptr Parser::ArithExpr() {
    NodeUptr left = this->MulExpr();
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;

    if (this->currTk_.type == TokenType::PLUS) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SUM, TokenType::TK_NULL, std::move(left), this->ArithExpr(), colno,
                                        lineno);
    }

    if (this->currTk_.type == TokenType::MINUS) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::SUB, TokenType::TK_NULL, std::move(left), this->ArithExpr(), colno,
                                        lineno);
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
            return std::make_unique<Binary>(NodeType::MUL, TokenType::TK_NULL, std::move(left), this->MulExpr(), colno,
                                            lineno);
        case TokenType::SLASH:
            this->Eat();
            return std::make_unique<Binary>(NodeType::DIV, TokenType::TK_NULL, std::move(left), this->MulExpr(), colno,
                                            lineno);
        case TokenType::SLASH_SLASH:
            this->Eat();
            return std::make_unique<Binary>(NodeType::INTEGER_DIV, TokenType::TK_NULL, std::move(left), this->MulExpr(),
                                            colno, lineno);
        case TokenType::PERCENT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::REMINDER, TokenType::TK_NULL, std::move(left), this->MulExpr(),
                                            colno, lineno);
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

    if (left->type == NodeType::SYNTAX_ERROR)
        return left;

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
            return std::make_unique<Binary>(NodeType::MEMBER,
                                            TokenType::TK_NULL,
                                            std::move(left),
                                            this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
        case TokenType::QUESTION_DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_SAFE,
                                            TokenType::TK_NULL,
                                            std::move(left),
                                            this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
        default:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_ASSERT,
                                            TokenType::TK_NULL,
                                            std::move(left),
                                            this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
    }
}

ast::NodeUptr Parser::ParseAtom() {
    // TODO: <list> | <maporset> | <arrow>
    NodeUptr tmp;

    if (this->Match(TokenType::FALSE, TokenType::TRUE))
        return std::make_unique<ast::Literal>(this->currTk_);

    if (this->currTk_.type == TokenType::NIL)
        return std::make_unique<ast::Literal>(this->currTk_);

    if ((tmp = this->ParseNumber()))
        return tmp;

    if ((tmp = this->ParseString()))
        return tmp;

    if ((tmp = this->ParseScope()))
        return tmp;

    return std::make_unique<SyntaxError>("unexpected: " + this->currTk_.value, this->currTk_.colno,
                                         this->currTk_.lineno);
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
    NodeUptr tmp;

    if (this->Match(TokenType::IDENTIFIER)) {
        tmp = std::make_unique<Scope>(this->currTk_);
        this->Eat();
    }

    while (this->Match(TokenType::SCOPE)) {
        this->Eat();
        if (!this->Match(TokenType::IDENTIFIER))
            return std::make_unique<SyntaxError>("expected identifier after ::",
                                                 this->currTk_.colno,
                                                 this->currTk_.lineno);
        CastNode<Scope>(tmp)->AddSegment(this->currTk_.value);
        this->Eat();
    }

    return tmp;
}
