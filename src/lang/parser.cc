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

    auto a = this->AtomExpr();
}

void Parser::Eat() {
    this->currTk_ = this->scanner_->Next();
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
                                            std::move(left),
                                            this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
        case TokenType::QUESTION_DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_SAFE,
                                            std::move(left),
                                            this->ParseScope(),
                                            this->currTk_.colno, this->currTk_.lineno);
        default:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_ASSERT,
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


