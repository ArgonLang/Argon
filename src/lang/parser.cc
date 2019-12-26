// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0
// 05/12/2019 <3

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

std::unique_ptr<ast::Block> Parser::Parse() {
    auto program = std::make_unique<ast::Block>(NodeType::PROGRAM, this->currTk_.colno, this->currTk_.colno);

    while (!this->Match(TokenType::END_OF_FILE))
        program->AddStmtOrExpr(this->Declaration());

    return program;
}

// *** DECLARATIONS ***

ast::NodeUptr Parser::Declaration() {
    if (this->Match(TokenType::IMPL))
        return this->ImplDecl();

    return this->AccessModifier();
}

ast::NodeUptr Parser::AccessModifier() {
    bool pub = false;

    if (this->Match(TokenType::PUB)) {
        this->Eat();
        pub = true;
    }

    if (this->Match(TokenType::LET))
        return this->ConstDecl(pub);
    else if (this->Match(TokenType::TRAIT))
        return this->TraitDecl(pub);

    return this->SmallDecl(pub);
}

ast::NodeUptr Parser::SmallDecl(bool pub) {
    switch (this->currTk_.type) {
        case TokenType::VAR:
            return this->VarDecl(pub);
        case TokenType::USING:
            return this->AliasDecl(pub);
        case TokenType::FUNC:
            return this->FuncDecl(pub);
        case TokenType::STRUCT:
            return this->StructDecl(pub);
        default:
            if (pub)
                throw SyntaxException("expected declaration after pub keyword, found statement", this->currTk_);
            return this->Statement();
    }
}

ast::NodeUptr Parser::AliasDecl(bool pub) {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    NodeUptr node;
    std::string name;

    this->Eat(TokenType::USING, "expected using");
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after alias keyword");
    this->Eat(TokenType::AS, "expected as after identifier in alias declaration");

    node = std::make_unique<Construct>(NodeType::ALIAS, name, nullptr, this->ParseScope(), colno, lineno);
    CastNode<Construct>(node)->pub = pub;

    return node;
}

ast::NodeUptr Parser::VarDecl(bool pub) {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    NodeUptr variable;
    bool atomic = false;
    bool weak = false;

    if (this->Match(TokenType::ATOMIC)) {
        this->Eat();
        atomic = true;
    }

    if (this->Match(TokenType::WEAK)) {
        this->Eat();
        weak = true;
    }

    if (!this->Match(TokenType::LET)) {
        if (atomic || weak)
            throw SyntaxException("expected variable declaration", this->currTk_);
        return nullptr;
    }

    this->Eat();
    variable = std::make_unique<Variable>(this->currTk_.value, nullptr, false, colno, lineno);
    this->Eat(TokenType::IDENTIFIER, "expected identifier after var declaration");
    CastNode<Variable>(variable)->annotation = this->VarAnnotation();
    if (this->Match(TokenType::EQUAL)) {
        this->Eat();
        CastNode<Variable>(variable)->value = this->TestList();
    }

    CastNode<Variable>(variable)->atomic = atomic;
    CastNode<Variable>(variable)->weak = weak;
    CastNode<Variable>(variable)->pub = pub;

    return variable;
}

ast::NodeUptr Parser::ConstDecl(bool pub) {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    std::string name;
    NodeUptr node;

    this->Eat(); // eat 'let' keyword
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after let declaration");
    this->Eat(TokenType::EQUAL, "expected = after identifier in let declaration");
    node = std::make_unique<Variable>(name, this->TestList(), true, colno, lineno);
    CastNode<Variable>(node)->pub = pub;

    return node;
}

ast::NodeUptr Parser::VarAnnotation() {
    if (this->Match(TokenType::COLON)) {
        this->Eat();
        return this->ParseScope();
    }
    return nullptr;
}

ast::NodeUptr Parser::FuncDecl(bool pub) {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    std::string name;
    std::list<NodeUptr> params;

    this->Eat(TokenType::FUNC, "expected func keyword");
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after func keyword");
    if (this->Match(TokenType::LEFT_ROUND)) {
        this->Eat();
        params = this->Param();
        this->Eat(TokenType::RIGHT_ROUND, "expected ) after params declaration");
    }

    return nullptr; // TODO: Block definition
}

std::list<NodeUptr> Parser::Param() {
    std::list<NodeUptr> params;
    NodeUptr tmp = this->Variadic();

    if (tmp != nullptr) {
        params.push_front(std::move(tmp));
        return params;
    }

    while (this->Match(TokenType::IDENTIFIER)) {
        tmp = this->ParseScope();
        if (tmp->type == NodeType::LITERAL)
            params.push_front(std::move(tmp));
        else
            throw SyntaxException("expected parameter name", this->currTk_);
        if (this->Match(TokenType::COMMA)) {
            this->Eat();
            if ((tmp = this->Variadic()) != nullptr) {
                params.push_front(std::move(tmp));
                break;
            }
        }
    }

    return params;
}

ast::NodeUptr Parser::Variadic() {
    if (this->Match(TokenType::ELLIPSIS)) {
        this->Eat();
        auto param = this->ParseScope();
        if (param->type == NodeType::LITERAL) {
            ((Node *) param.get())->type = NodeType::VARIADIC;
            return param;
        }
        throw SyntaxException("expected parameter name", this->currTk_);
    }
    return nullptr;
}

ast::NodeUptr Parser::StructDecl(bool pub) {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    std::string name;
    NodeUptr impl;

    this->Eat(TokenType::STRUCT, "expected struct");
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after struct keyword");

    if (this->Match(TokenType::IMPL)) {
        this->Eat();
        impl = this->TraitList();
    }

    impl = std::make_unique<Construct>(NodeType::STRUCT, name, std::move(impl), this->StructBlock(), colno, lineno);
    CastNode<Construct>(impl)->pub = pub;

    return impl;
}

ast::NodeUptr Parser::StructBlock() {
    NodeUptr block = std::make_unique<ast::Block>(NodeType::STRUCT_BLOCK, this->currTk_.colno, this->currTk_.colno);

    this->Eat(TokenType::LEFT_BRACES, "expected { after struct declaration");

    while (!this->Match(TokenType::RIGHT_BRACES)) {
        bool pub = false;

        if (this->Match(TokenType::PUB)) {
            pub = true;
            this->Eat();
        }

        switch (this->currTk_.type) {
            case TokenType::LET:
                CastNode<Block>(block)->AddStmtOrExpr(this->ConstDecl(pub));
                break;
            case TokenType::VAR:
                CastNode<Block>(block)->AddStmtOrExpr(this->VarDecl(pub));
                break;
            case TokenType::FUNC:
                // TODO: impl function
            default:
                throw SyntaxException("expected variable, constant or function declaration", this->currTk_);
        }
    }

    this->Eat();
    return block;
}

ast::NodeUptr Parser::TraitDecl(bool pub) {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    std::string name;
    NodeUptr impl;

    this->Eat(TokenType::TRAIT, "expected trait");
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after trait keyword");

    if (this->Match(TokenType::COLON)) {
        this->Eat();
        impl = this->TraitList();
    }

    impl = std::make_unique<Construct>(NodeType::TRAIT, name, std::move(impl), this->TraitBlock(), colno, lineno);
    CastNode<Construct>(impl)->pub = pub;

    return impl;
}

ast::NodeUptr Parser::TraitBlock() {
    NodeUptr block = std::make_unique<ast::Block>(NodeType::TRAIT_BLOCK, this->currTk_.colno, this->currTk_.colno);

    this->Eat(TokenType::LEFT_BRACES, "expected { after impl declaration");

    while (!this->Match(TokenType::RIGHT_BRACES)) {
        bool pub = false;

        if (this->Match(TokenType::PUB)) {
            pub = true;
            this->Eat();
        }

        switch (this->currTk_.type) {
            case TokenType::LET:
                CastNode<Block>(block)->AddStmtOrExpr(this->ConstDecl(pub));
                break;
            case TokenType::FUNC:
                // TODO: impl function
            default:
                throw SyntaxException("expected constant or function declaration", this->currTk_);
        }
    }

    this->Eat();
    return block;
}

ast::NodeUptr Parser::TraitList() {
    NodeUptr impls = std::make_unique<List>(NodeType::TRAIT_LIST, this->currTk_.colno, this->currTk_.lineno);

    CastNode<List>(impls)->AddExpression(this->ParseScope());

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        CastNode<List>(impls)->AddExpression(this->ParseScope());
    }

    return impls;
}

ast::NodeUptr Parser::ImplDecl() {
    unsigned colno = this->currTk_.colno;
    unsigned lineno = this->currTk_.lineno;
    NodeUptr implName;
    NodeUptr implTarget;

    this->Eat(TokenType::IMPL, "expected impl");
    implName = this->ParseScope();

    if (this->Match(TokenType::FOR)) {
        this->Eat();
        implTarget = this->ParseScope();
    }

    return std::make_unique<Impl>(std::move(implName), std::move(implTarget), this->TraitBlock(), colno, lineno);
}

// *** STATEMENTS ***

ast::NodeUptr Parser::Statement() {
    if (!this->TokenInRange(TokenType::KEYWORD_BEGIN, TokenType::KEYWORD_END))
        return this->Expression();

    switch (this->currTk_.type) {
        case TokenType::DEFER:
        case TokenType::SPAWN:
        case TokenType::RETURN:
        case TokenType::IMPORT:
            break;
        case TokenType::IF:
        default:
            break; // ??
    }
}

// *** EXPRESSIONS ***

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
