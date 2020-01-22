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
    this->Eat();
}

void Parser::EatTerm(bool must_eat) {
    this->EatTerm(must_eat, TokenType::END_OF_FILE);
}

void Parser::EatTerm(bool must_eat, TokenType stop_token) {
    if (this->Match(stop_token))
        return;

    while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON)) {
        this->Eat();
        must_eat = false;
    }

    if (must_eat)
        throw SyntaxException("expected EOL or SEMICOLON", this->currTk_);
}

std::unique_ptr<Program> Parser::Parse() {
    auto program = std::make_unique<Program>(this->currTk_.start);

    this->EatTerm(false);

    while (!this->Match(TokenType::END_OF_FILE)) {
        program->AddStatement(this->Declaration());
        this->EatTerm(true);
    }

    program->SetEndPos(this->currTk_.end);

    return program;
}

// *** DECLARATIONS ***

ast::NodeUptr Parser::Declaration() {
    if (this->Match(TokenType::IMPL))
        return this->ImplDecl();

    return this->AccessModifier();
}

ast::NodeUptr Parser::AccessModifier() {
    Pos start = this->currTk_.start;
    bool pub = false;
    NodeUptr expr;

    if (this->Match(TokenType::PUB)) {
        this->Eat();
        pub = true;
    }

    if (this->Match(TokenType::LET))
        expr = this->ConstDecl(pub);
    else if (this->Match(TokenType::TRAIT))
        expr = this->TraitDecl(pub);
    else
        expr = this->SmallDecl(pub);

    if (pub)
        CastNode<Node>(expr)->start = start;

    return expr;
}

ast::NodeUptr Parser::SmallDecl(bool pub) {
    switch (this->currTk_.type) {
        case TokenType::ATOMIC:
        case TokenType::WEAK:
        case TokenType::VAR:
            return this->VarModifier(pub);
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
    Pos start = this->currTk_.start;
    std::string name;

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after alias keyword");
    this->Eat(TokenType::AS, "expected as after identifier in alias declaration");
    return std::make_unique<Alias>(name, this->ParseScope(), pub, start);
}

ast::NodeUptr Parser::VarModifier(bool pub) {
    bool atomic = false;
    bool weak = false;
    Pos start = this->currTk_.start;
    NodeUptr variable;

    if (this->Match(TokenType::ATOMIC)) {
        this->Eat();
        atomic = true;
    }

    if (this->Match(TokenType::WEAK)) {
        this->Eat();
        weak = true;
    }

    if (this->Match(TokenType::LET))
        throw SyntaxException("expected var keyword", this->currTk_);

    variable = this->VarDecl(pub);

    CastNode<Variable>(variable)->start = start;
    CastNode<Variable>(variable)->atomic = atomic;
    CastNode<Variable>(variable)->weak = weak;

    return variable;
}

ast::NodeUptr Parser::VarDecl(bool pub) {
    Pos start = this->currTk_.start;
    NodeUptr value;
    std::unique_ptr<Variable> variable;

    this->Eat();
    variable = std::make_unique<Variable>(this->currTk_.value, pub, false);
    variable->start = start;
    variable->end = this->currTk_.end;

    this->Eat(TokenType::IDENTIFIER, "expected identifier after var keyword");

    if (this->Match(TokenType::COLON)) {
        this->Eat();
        variable->annotation = this->ParseScope();
        variable->end = variable->annotation->end;
    }

    if (this->Match(TokenType::EQUAL)) {
        this->Eat();
        value = this->TestList();
        variable->end = value->end;
        variable->value = std::move(value);
    }
    return variable;
}

ast::NodeUptr Parser::ConstDecl(bool pub) {
    Pos start = this->currTk_.start;
    std::string name;
    std::unique_ptr<Variable> constant;

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after let keyword");
    this->Eat(TokenType::EQUAL, "expected = after identifier in let declaration");
    constant = std::make_unique<Variable>(name, this->TestList(), pub, true);
    constant->start = start;

    return constant;
}

ast::NodeUptr Parser::FuncDecl(bool pub) {
    Pos start = this->currTk_.start;
    std::string name;
    std::list<NodeUptr> params;

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after func keyword");
    if (this->Match(TokenType::LEFT_ROUND)) {
        this->Eat();
        params = this->Param();
        this->Eat(TokenType::RIGHT_ROUND, "expected ) after function params");
    }

    return std::make_unique<Function>(name, std::move(params), this->Block(), pub, start);
}

std::list<NodeUptr> Parser::Param() {
    std::list<NodeUptr> params;
    NodeUptr tmp = this->Variadic();

    if (tmp != nullptr) {
        params.push_back(std::move(tmp));
        return params;
    }

    while (this->Match(TokenType::IDENTIFIER)) {
        tmp = this->ParseScope();
        if (tmp->type == NodeType::IDENTIFIER)
            params.push_back(std::move(tmp));
        else
            throw SyntaxException("expected parameter name", this->currTk_);
        if (this->Match(TokenType::COMMA)) {
            this->Eat();
            if ((tmp = this->Variadic()) != nullptr) {
                params.push_back(std::move(tmp));
                break;
            }
        }
    }

    return params;
}

ast::NodeUptr Parser::Variadic() {
    Pos start = this->currTk_.start;
    std::unique_ptr<Identifier> id;

    if (this->Match(TokenType::ELLIPSIS)) {
        this->Eat();
        if (!this->Match(TokenType::IDENTIFIER))
            throw SyntaxException("expected identifier after ...(ellipsis) operator", this->currTk_);
        id = std::make_unique<Identifier>(this->currTk_);
        id->start = start;
        id->rest_element = true;
        this->Eat();
    }

    return id;
}

ast::NodeUptr Parser::StructDecl(bool pub) {
    Pos start = this->currTk_.start;
    std::string name;
    std::list<NodeUptr> impls;

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after struct keyword");

    if (this->Match(TokenType::IMPL)) {
        this->Eat();
        impls = this->TraitList();
    }

    return std::make_unique<Construct>(NodeType::STRUCT, name, impls, this->StructBlock(), pub, start);
}

ast::NodeUptr Parser::StructBlock() {
    auto block = std::make_unique<ast::Block>(NodeType::STRUCT_BLOCK, this->currTk_.start);
    NodeUptr stmt;

    this->Eat(TokenType::LEFT_BRACES, "expected { after struct declaration");

    this->EatTerm(false);

    while (!this->Match(TokenType::RIGHT_BRACES)) {
        Pos start = this->currTk_.start;
        bool pub = false;

        if (this->Match(TokenType::PUB)) {
            pub = true;
            this->Eat();
        }

        switch (this->currTk_.type) {
            case TokenType::LET:
                stmt = this->ConstDecl(pub);
                break;
            case TokenType::ATOMIC:
            case TokenType::WEAK:
            case TokenType::VAR:
                stmt = this->VarModifier(pub);
                break;
            case TokenType::FUNC:
                stmt = this->FuncDecl(pub);
                break;
            default:
                throw SyntaxException("expected variable, constant or function declaration", this->currTk_);
        }

        CastNode<Node>(stmt)->start = start;
        block->AddStmtOrExpr(std::move(stmt));
        this->EatTerm(true, TokenType::RIGHT_BRACES);
    }

    block->SetEndPos(this->currTk_.end);
    this->Eat();
    return block;
}

ast::NodeUptr Parser::TraitDecl(bool pub) {
    Pos start = this->currTk_.start;
    std::string name;
    std::list<NodeUptr> impls;

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after trait keyword");

    if (this->Match(TokenType::COLON)) {
        this->Eat();
        impls = this->TraitList();
    }

    return std::make_unique<Construct>(NodeType::TRAIT, name, impls, this->TraitBlock(), pub, start);
}

ast::NodeUptr Parser::TraitBlock() {
    auto block = std::make_unique<ast::Block>(NodeType::TRAIT_BLOCK, this->currTk_.start);
    NodeUptr stmt;

    this->Eat(TokenType::LEFT_BRACES, "expected { after impl declaration");

    this->EatTerm(false);

    while (!this->Match(TokenType::RIGHT_BRACES)) {
        Pos start = this->currTk_.start;
        bool pub = false;

        if (this->Match(TokenType::PUB)) {
            pub = true;
            this->Eat();
        }

        switch (this->currTk_.type) {
            case TokenType::LET:
                stmt = this->ConstDecl(pub);
                break;
            case TokenType::FUNC:
                stmt = this->FuncDecl(pub);
                break;
            default:
                throw SyntaxException("expected constant or function declaration", this->currTk_);
        }

        CastNode<Node>(stmt)->start = start;
        block->AddStmtOrExpr(std::move(stmt));
        this->EatTerm(true, TokenType::RIGHT_BRACES);
    }

    block->SetEndPos(this->currTk_.end);
    this->Eat();
    return block;
}

std::list<NodeUptr> Parser::TraitList() {
    std::list<NodeUptr> impls;

    impls.push_back(this->ParseScope());

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        impls.push_back(this->ParseScope());
    }

    return impls;
}

ast::NodeUptr Parser::ImplDecl() {
    Pos start = this->currTk_.start;
    NodeUptr implName;
    NodeUptr implTarget;

    this->Eat();
    implName = this->ParseScope();

    if (this->Match(TokenType::FOR)) {
        this->Eat();
        implTarget = this->ParseScope();
    }

    if (implTarget != nullptr)
        return std::make_unique<Impl>(std::move(implTarget), std::move(implName), this->TraitBlock(), start);

    return std::make_unique<Impl>(std::move(implName), this->TraitBlock(), start);
}

// *** STATEMENTS ***

ast::NodeUptr Parser::Statement() {
    Pos start = this->currTk_.start;
    NodeUptr tmp;

    if (!this->TokenInRange(TokenType::KEYWORD_BEGIN, TokenType::KEYWORD_END))
        return this->Expression();

    switch (this->currTk_.type) {
        case TokenType::DEFER:
            this->Eat();
            return std::make_unique<Unary>(NodeType::DEFER, this->AtomExpr(), start);
        case TokenType::SPAWN:
            this->Eat();
            return std::make_unique<Unary>(NodeType::SPAWN, this->AtomExpr(), start);
        case TokenType::RETURN:
            this->Eat();
            return std::make_unique<Unary>(NodeType::RETURN, this->TestList(), start);
        case TokenType::IMPORT:
            return this->ImportStmt();
        case TokenType::FROM:
            return this->FromImportStmt();
        case TokenType::FOR:
            return this->ForStmt();
        case TokenType::LOOP:
            return this->LoopStmt();
        case TokenType::IF:
            return this->IfStmt();
        case TokenType::SWITCH:
            return this->SwitchStmt();
        case TokenType::BREAK:
        case TokenType::CONTINUE:
        case TokenType::FALLTHROUGH:
        case TokenType::GOTO:
            return this->JmpStmt();
        default:
            throw SyntaxException("expected statement", this->currTk_);
    }
}

ast::NodeUptr Parser::ImportStmt() {
    NodeUptr import = std::make_unique<Import>(this->currTk_.start, this->currTk_.end);

    this->Eat(TokenType::IMPORT, "expected import keyword");

    CastNode<Import>(import)->AddName(this->DottedAsName());

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        CastNode<Import>(import)->AddName(this->DottedAsName());
    }

    return import;
}

ast::NodeUptr Parser::FromImportStmt() {
    NodeUptr import = std::make_unique<Import>(this->currTk_.start, this->currTk_.end);

    this->Eat(TokenType::FROM, "expected from keyword");

    CastNode<Import>(import)->module = this->DottedName();

    this->Eat(TokenType::IMPORT, "expected import keyword");

    CastNode<Import>(import)->AddName(this->ImportAsName());
    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        CastNode<Import>(import)->AddName(this->ImportAsName());
    }

    return import;
}

ast::NodeUptr Parser::ImportAsName() {
    unsigned colno = this->currTk_.start;
    unsigned lineno = this->currTk_.end;
    std::string name = this->currTk_.value;
    std::string alias;

    this->Eat(TokenType::IDENTIFIER, "expected name");

    if (this->Match(TokenType::AS)) {
        this->Eat();
        alias = this->currTk_.value;
        this->Eat(TokenType::IDENTIFIER, "expected alias name");
    }

    return std::make_unique<ImportAlias>(name, alias, colno, lineno);
}

ast::NodeUptr Parser::DottedAsName() {
    unsigned colno = this->currTk_.start;
    unsigned lineno = this->currTk_.end;
    std::string dotted;
    std::string alias;

    dotted = this->DottedName();

    if (this->Match(TokenType::AS)) {
        this->Eat();
        alias = this->currTk_.value;
        this->Eat(TokenType::IDENTIFIER, "expected alias name");
    }

    return std::make_unique<ImportAlias>(dotted, alias, colno, lineno);
}

std::string Parser::DottedName() {
    std::string dotted = this->currTk_.value;

    this->Eat(TokenType::IDENTIFIER, "expected name");

    while (this->Match(TokenType::DOT)) {
        this->Eat();
        dotted += "." + this->currTk_.value;
    }

    return dotted;
}

ast::NodeUptr Parser::ForStmt() {
    unsigned colno = this->currTk_.start;
    unsigned lineno = this->currTk_.end;
    NodeUptr init;
    NodeUptr test;
    NodeUptr inc;
    NodeUptr body;

    this->Eat(TokenType::FOR, "expected for keyword");

    if (!this->Match(TokenType::SEMICOLON)) {
        if (this->Match(TokenType::VAR))
            init = this->VarDecl(false);
        else
            init = this->Expression();
    }

    if (this->Match(TokenType::IN)) {
        if (init->type != NodeType::LITERAL || CastNode<Literal>(init)->kind != TokenType::IDENTIFIER)
            throw SyntaxException("expected identifier or tuple of identifier", this->currTk_);

        if (init->type == NodeType::TUPLE) {
            for (auto &item : CastNode<List>(init)->expressions) {
                if (item->type != NodeType::LITERAL || CastNode<Literal>(item)->kind != TokenType::IDENTIFIER)
                    throw SyntaxException("expected identifier or tuple of identifier", this->currTk_);
            }
        }
        this->Eat();
        return std::make_unique<For>(std::move(init), this->Expression(), this->Block(), colno, lineno);
    }

    this->Eat(TokenType::SEMICOLON, "expected ; after init");

    test = this->Test();

    this->Eat(TokenType::SEMICOLON, "expected ; after test condition");

    inc = this->Expression();

    return std::make_unique<For>(std::move(init), std::move(test), std::move(inc), this->Block(), colno, lineno);
}

ast::NodeUptr Parser::LoopStmt() {
    unsigned colno = this->currTk_.start;
    unsigned lineno = this->currTk_.end;

    this->Eat(TokenType::LOOP, "expected loop keyword");

    if (this->Match(TokenType::LEFT_BRACES))
        return std::make_unique<Loop>(nullptr, this->Block(), colno, lineno);

    return std::make_unique<Loop>(this->Test(), this->Block(), colno, lineno);
}

ast::NodeUptr Parser::IfStmt() {
    Pos start = this->currTk_.start;
    std::unique_ptr<If> ifs;
    NodeUptr test;

    this->Eat();

    test = this->Test();
    ifs = std::make_unique<If>(std::move(test), this->Block(), start);

    if (this->Match(TokenType::ELIF)) {
        ifs->orelse = this->IfStmt();
        ifs->end = ifs->orelse->end;
    } else if (this->Match(TokenType::ELSE)) {
        this->Eat();
        ifs->orelse = this->Block();
        ifs->end = ifs->orelse->end;
    }

    return ifs;
}

ast::NodeUptr Parser::SwitchStmt() {
    Pos start = this->currTk_.start;
    bool def = false;
    NodeUptr test;

    this->Eat();

    if (!this->Match(TokenType::LEFT_BRACES))
        test = this->Test();

    auto sw = std::make_unique<Switch>(std::move(test), start);

    this->Eat(TokenType::LEFT_BRACES, "expected { after switch declaration");

    this->EatTerm(false);

    while (this->Match(TokenType::CASE, TokenType::DEFAULT)) {
        if (this->currTk_.type == TokenType::DEFAULT) {
            if (def)
                throw SyntaxException("default case already defined", this->currTk_);
            def = true;
        }
        sw->AddCase(this->SwitchCase());
        this->EatTerm(false);
    }

    sw->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_BRACES, "expected } after switch declaration");

    return sw;
}

ast::NodeUptr Parser::SwitchCase() {
    auto swc = std::make_unique<Case>(this->currTk_.start);
    std::unique_ptr<ast::Block> body;
    NodeUptr tmp;

    if (this->Match(TokenType::DEFAULT)) {
        this->Eat();
    } else if (this->Match(TokenType::CASE)) {
        this->Eat();
        this->Test();
        while (this->Match(TokenType::PIPE)) {
            this->Eat();
            swc->AddCondition(this->Test());
        }
    } else
        throw SyntaxException("expected case or default label", this->currTk_);

    this->Eat(TokenType::COLON, "expected : after default/case label");

    this->EatTerm(false);

    body = std::make_unique<ast::Block>(NodeType::BLOCK, this->currTk_.start);

    while (!this->Match(TokenType::CASE, TokenType::DEFAULT, TokenType::RIGHT_BRACES)) {
        tmp = this->SmallDecl(false);
        body->SetEndPos(tmp->end);
        body->AddStmtOrExpr(std::move(tmp));
        if (!this->Match(TokenType::CASE, TokenType::DEFAULT))
            this->EatTerm(true, TokenType::RIGHT_BRACES);
    }

    swc->body = std::move(body);

    return swc;
}

ast::NodeUptr Parser::JmpStmt() {
    Pos start = this->currTk_.start;
    NodeUptr label;

    if (this->Match(TokenType::FALLTHROUGH)) {
        label = std::make_unique<Unary>(NodeType::FALLTHROUGH, nullptr, start);
        CastNode<Node>(label)->end = this->currTk_.end;
        this->Eat();
        return label;
    }

    switch (this->currTk_.type) {
        case TokenType::BREAK:
            this->Eat();
            label = std::make_unique<Unary>(NodeType::BREAK, nullptr, start);
            break;
        case TokenType::CONTINUE:
            this->Eat();
            label = std::make_unique<Unary>(NodeType::CONTINUE, nullptr, start);
            break;
        case TokenType::GOTO:
            this->Eat();
            label = std::make_unique<Unary>(NodeType::GOTO, nullptr, start);
            break;
        default:
            // should never get here!
            throw SyntaxException("invalid jmp syntax", this->currTk_);
    }

    if (this->Match(TokenType::IDENTIFIER)) {
        NodeUptr tmp = this->ParseScope();
        if (tmp->type == NodeType::IDENTIFIER) {
            CastNode<Unary>(label)->end = tmp->end;
            CastNode<Unary>(label)->expr = std::move(tmp);
        } else
            throw SyntaxException("expected label name", this->currTk_);
    }

    return label;
}

ast::NodeUptr Parser::Block() {
    auto body = std::make_unique<ast::Block>(NodeType::BLOCK, this->currTk_.start);

    this->Eat(TokenType::LEFT_BRACES, "expected {");

    this->EatTerm(false);

    while (!this->Match(TokenType::RIGHT_BRACES)) {
        body->AddStmtOrExpr(this->SmallDecl(false));
        this->EatTerm(true, TokenType::RIGHT_BRACES);
    }

    body->SetEndPos(this->currTk_.end);
    this->Eat();

    return body;
}

// *** EXPRESSIONS ***

ast::NodeUptr Parser::Expression() {
    NodeUptr left = this->TestList();

    if (!this->Match(TokenType::EQUAL)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::ASSIGN, std::move(left), this->TestList());
}

ast::NodeUptr Parser::TestList() {
    NodeUptr left = this->Test();

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        if (left->type != NodeType::TUPLE) {
            auto tuple = std::make_unique<List>(NodeType::TUPLE, left->start);
            tuple->AddExpression(std::move(left));
            left = std::move(tuple);
        }
        CastNode<List>(left)->AddExpression(this->Test());
    }

    return left;
}

ast::NodeUptr Parser::Test() {
    NodeUptr left = this->OrTest();
    Pos start = left->start;
    Pos end;
    NodeUptr lt;
    NodeUptr rt;

    if (this->Match(TokenType::ELVIS)) {
        this->Eat();
        return std::make_unique<Binary>(NodeType::ELVIS, std::move(left), this->TestList());
    } else if (this->Match(TokenType::QUESTION)) {
        this->Eat();
        lt = this->TestList();
        this->Eat(TokenType::COLON, "expected : in ternary operator");
        rt = this->TestList();
        end = rt->end;
        return std::make_unique<If>(std::move(left), std::move(lt), std::move(rt), start, end);
    }

    return left;
}

ast::NodeUptr Parser::OrTest() {
    NodeUptr left = this->AndTest();

    if (!this->Match(TokenType::OR)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::TEST, TokenType::OR, std::move(left), this->OrTest());
}

ast::NodeUptr Parser::AndTest() {
    NodeUptr left = this->OrExpr();

    if (!this->Match(TokenType::AND)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::TEST, TokenType::AND, std::move(left), this->AndTest());
}

ast::NodeUptr Parser::OrExpr() {
    NodeUptr left = this->XorExpr();

    if (!this->Match(TokenType::PIPE)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::LOGICAL, TokenType::PIPE, std::move(left), this->OrExpr());
}

ast::NodeUptr Parser::XorExpr() {
    NodeUptr left = this->AndExpr();

    if (!this->Match(TokenType::CARET)) return left;
    this->Eat();
    return std::make_unique<Binary>(NodeType::LOGICAL, TokenType::CARET, std::move(left), this->XorExpr());
}

ast::NodeUptr Parser::AndExpr() {
    NodeUptr left = this->EqualityExpr();

    if (!this->Match(TokenType::AMPERSAND)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::LOGICAL, TokenType::AMPERSAND, std::move(left), this->AndExpr());
}

ast::NodeUptr Parser::EqualityExpr() {
    NodeUptr left = this->RelationalExpr();
    TokenType kind = this->currTk_.type;

    if (!this->Match(TokenType::EQUAL_EQUAL, TokenType::NOT_EQUAL)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::EQUALITY, kind, std::move(left), this->RelationalExpr());
}

ast::NodeUptr Parser::RelationalExpr() {
    NodeUptr left = this->ShiftExpr();
    TokenType kind = this->currTk_.type;

    if (!this->TokenInRange(TokenType::RELATIONAL_BEGIN, TokenType::RELATIONAL_END)) return left;

    this->Eat();
    return std::make_unique<Binary>(NodeType::RELATIONAL, kind, std::move(left), this->ShiftExpr());
}

ast::NodeUptr Parser::ShiftExpr() {
    NodeUptr left = this->ArithExpr();
    TokenType type = this->currTk_.type;

    switch (this->currTk_.type) {
        case TokenType::SHL:
        case TokenType::SHR:
            this->Eat();
            return std::make_unique<Binary>(NodeType::BINARY_OP, type, std::move(left), this->ShiftExpr());
        default:
            return left;
    }
}

ast::NodeUptr Parser::ArithExpr() {
    NodeUptr left = this->MulExpr();
    TokenType type = this->currTk_.type;

    switch (this->currTk_.type) {
        case TokenType::PLUS:
        case TokenType::MINUS:
            this->Eat();
            return std::make_unique<Binary>(NodeType::BINARY_OP, type, std::move(left), this->ArithExpr());
        default:
            return left;
    }
}

ast::NodeUptr Parser::MulExpr() {
    NodeUptr left = this->UnaryExpr();
    TokenType type = this->currTk_.type;

    switch (this->currTk_.type) {
        case TokenType::ASTERISK:
        case TokenType::SLASH:
        case TokenType::SLASH_SLASH:
        case TokenType::PERCENT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::BINARY_OP, type, std::move(left), this->MulExpr());
        default:
            return left;
    }
}

ast::NodeUptr Parser::UnaryExpr() {
    Pos start = this->currTk_.start;
    TokenType type = this->currTk_.type;
    Pos end;
    NodeUptr expr;

    switch (this->currTk_.type) {
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
        case TokenType::PLUS:
        case TokenType::MINUS:
            this->Eat();
            expr = this->UnaryExpr();
            end = expr->end;
            return std::make_unique<Unary>(NodeType::UNARY_OP, type, std::move(expr), start, end);
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            this->Eat();
            expr = this->UnaryExpr();
            end = expr->end;
            return std::make_unique<Update>(std::move(expr), type, true, start, end);
        default:
            return this->AtomExpr();
    }
}

ast::NodeUptr Parser::AtomExpr() {
    auto left = this->ParseAtom();
    Pos end = left->end;

    while (end > 0) {
        end = left->end;
        switch (this->currTk_.type) {
            case TokenType::LEFT_ROUND:
                left = this->ParseArguments(std::move(left));
                break;
            case TokenType::LEFT_SQUARE:
                left = std::make_unique<Binary>(NodeType::SUBSCRIPT, std::move(left), this->ParseSubscript());
                break;
            case TokenType::DOT:
            case TokenType::QUESTION_DOT:
            case TokenType::EXCLAMATION_DOT:
                left = this->MemberAccess(std::move(left));
                break;
            case TokenType::PLUS_PLUS:
            case TokenType::MINUS_MINUS:
                left = std::make_unique<Update>(std::move(left), this->currTk_.type, false, end);
                this->Eat();
                break;
            default:
                end = 0;
        }
    }
    return left;
}

ast::NodeUptr Parser::ParseArguments(NodeUptr left) {
    auto call = std::make_unique<Call>(std::move(left));
    Pos start;
    NodeUptr tmp;

    this->Eat();

    if (this->Match(TokenType::RIGHT_ROUND)) {
        call->end = this->currTk_.end;
        this->Eat();
        return call;
    }

    tmp = this->Test();
    start = tmp->start;
    if (this->Match(TokenType::ELLIPSIS)) {
        tmp = std::make_unique<Unary>(NodeType::ELLIPSIS, std::move(tmp), start);
        CastNode<Node>(tmp)->end = this->currTk_.end;
        this->Eat();
        call->AddArgument(std::move(tmp));
    } else {
        while (this->Match(TokenType::COMMA)) {
            this->Eat();
            tmp = this->Test();
            start = tmp->start;
            if (this->Match(TokenType::ELLIPSIS)) {
                tmp = std::make_unique<Unary>(NodeType::ELLIPSIS, std::move(tmp), start);
                CastNode<Node>(tmp)->end = this->currTk_.end;
                this->Eat();
                call->AddArgument(std::move(tmp));
                break;
            }
            call->AddArgument(std::move(tmp));
        }
    }

    call->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_ROUND, "expected ) after function call");

    return call;
}

ast::NodeUptr Parser::ParseSubscript() {
    NodeUptr low;
    NodeUptr high;
    NodeUptr step;

    this->Eat();

    low = this->Test();

    if (this->Match(TokenType::COLON)) {
        this->Eat();
        high = this->Test();
        if (this->Match(TokenType::COLON)) {
            this->Eat();
            step = this->Test();
        }
    }

    auto slice = std::make_unique<Slice>(std::move(low), std::move(high), std::move(step));
    slice->end = this->currTk_.end;

    this->Eat(TokenType::RIGHT_SQUARE, "expected ]");
    return slice;
}

ast::NodeUptr Parser::MemberAccess(ast::NodeUptr left) {
    switch (this->currTk_.type) {
        case TokenType::DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER, std::move(left), this->ParseScope());
        case TokenType::QUESTION_DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_SAFE, std::move(left), this->ParseScope());
        case TokenType::EXCLAMATION_DOT:
            this->Eat();
            return std::make_unique<Binary>(NodeType::MEMBER_ASSERT, std::move(left), this->ParseScope());
        default:
            throw SyntaxException("expected . or ?. or !. operator", this->currTk_);
    }
}

ast::NodeUptr Parser::ParseAtom() {
    NodeUptr tmp;

    // Parse number
    if (this->TokenInRange(TokenType::NUMBER_BEGIN, TokenType::NUMBER_END)) {
        tmp = std::make_unique<ast::Literal>(this->currTk_);
        this->Eat();
        return tmp;
    }

    // Parse string
    if (this->TokenInRange(TokenType::STRING_BEGIN, TokenType::STRING_END)) {
        tmp = std::make_unique<ast::Literal>(this->currTk_);
        this->Eat();
        return tmp;
    }

    switch (this->currTk_.type) {
        case TokenType::FALSE:
        case TokenType::TRUE:
        case TokenType::NIL:
            tmp = std::make_unique<ast::Literal>(this->currTk_);
            this->Eat();
            return tmp;
        case TokenType::LEFT_ROUND:
            return this->ParseArrowOrTuple();
        case TokenType::LEFT_SQUARE:
            return this->ParseList();
        case TokenType::LEFT_BRACES:
            return this->ParseMapOrSet();
        default:
            return this->ParseScope();
    }
}

ast::NodeUptr Parser::ParseArrowOrTuple() {
    Pos start = this->currTk_.start;
    auto tuple = std::make_unique<List>(NodeType::TUPLE, start);
    std::list<NodeUptr> params;
    bool mustFn = false;

    this->Eat();

    if (!this->Match(TokenType::RIGHT_ROUND)) {
        params = this->ParsePeap();
        for (auto &item : params) {
            if (item->type == NodeType::IDENTIFIER) {
                if (CastNode<Identifier>(item)->rest_element) {
                    mustFn = true;
                    break;
                }
                continue;
            }
            tuple->end = this->currTk_.end;
            this->Eat(TokenType::RIGHT_ROUND, "expected ) after parenthesized expression");
            tuple->expressions = std::move(params);
            return tuple;
        }
    }

    tuple->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_ROUND, "expected )");

    if (this->Match(TokenType::ARROW)) {
        this->Eat();
        return std::make_unique<Function>(std::move(params), this->Block(), start);
    }

    if (mustFn)
        this->Eat(TokenType::ARROW, "expected arrow function declaration");

    // it's definitely a parenthesized expression
    tuple->expressions = std::move(params);
    return tuple;
}

std::list<ast::NodeUptr> Parser::ParsePeap() {
    std::list<ast::NodeUptr> params;
    NodeUptr tmp = this->Variadic();

    if (tmp != nullptr) {
        params.push_back(std::move(tmp));
        return params;
    }

    params.push_back(this->Test());
    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        if ((tmp = this->Variadic()) != nullptr) {
            params.push_back(std::move(tmp));
            break;
        }
        params.push_back(this->Test());
    }

    return params;
}

ast::NodeUptr Parser::ParseList() {
    auto list = std::make_unique<List>(NodeType::LIST, this->currTk_.start);

    this->Eat();

    if (this->Match(TokenType::RIGHT_SQUARE)) {
        list->end = this->currTk_.end;
        this->Eat();
        return list;
    }

    list->AddExpression(this->Test());

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        list->AddExpression(this->Test());
    }

    list->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_SQUARE, "expected ] after list definition");

    return list;
}

ast::NodeUptr Parser::ParseMapOrSet() {
    auto ms = std::make_unique<List>(NodeType::MAP, this->currTk_.start);

    this->Eat();

    if (this->Match(TokenType::RIGHT_BRACES)) {
        ms->end = this->currTk_.end;
        this->Eat();
        return ms;
    }

    ms->AddExpression(this->Test());

    if (this->Match(TokenType::COLON))
        this->ParseMap(ms);
    else if (this->Match(TokenType::COMMA)) {
        ms->type = NodeType::SET;
        do {
            this->Eat();
            ms->AddExpression(this->Test());
        } while (this->Match(TokenType::COMMA));
    } else
        ms->type = NodeType::SET;

    ms->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_BRACES, "expected } after map/set definition");

    return ms;
}

void Parser::ParseMap(std::unique_ptr<ast::List> &map) {
    do {
        this->Eat(TokenType::COLON, "missing value after key in map definition");
        map->AddExpression(this->Test());
        if (!this->Match(TokenType::COMMA))
            break;
        this->Eat();
        map->AddExpression(this->Test());
    } while (true);
}

ast::NodeUptr Parser::ParseScope() {
    NodeUptr scope;
    Pos end;

    if (this->Match(TokenType::IDENTIFIER)) {
        scope = std::make_unique<Identifier>(this->currTk_);
        this->Eat();

        if (this->Match(TokenType::SCOPE)) {
            auto tmp = std::make_unique<Scope>(scope->start);
            tmp->AddSegment(CastNode<Identifier>(scope)->value);
            do {
                this->Eat();
                if (!this->Match(TokenType::IDENTIFIER))
                    throw SyntaxException("expected identifier after ::(scope resolution)", this->currTk_);
                tmp->AddSegment(this->currTk_.value);
                end = this->currTk_.end;
                this->Eat();
            } while (this->Match(TokenType::SCOPE));
            tmp->end = end;
            scope = std::move(tmp);
        }
        return scope;
    }

    throw SyntaxException("expected identifier or expression", this->currTk_);
}
