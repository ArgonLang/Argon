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
    this->filename_ = std::move(filename);
}

std::unique_ptr<Program> Parser::Parse() {
    auto program = std::make_unique<Program>(this->filename_, this->currTk_.start);

    this->Eat(); // Init parser

    this->EatTerm(false);

    while (!this->Match(TokenType::END_OF_FILE)) {
        program->AddStatement(this->Declaration());
        this->EatTerm(true);
    }

    program->SetEndPos(this->currTk_.end);
    program->docs = std::move(this->comments_);

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
    Pos end;
    NodeUptr name;
    NodeUptr target;

    this->Eat();

    name = std::make_unique<Identifier>(this->currTk_);
    this->Eat(TokenType::IDENTIFIER, "expected identifier after alias keyword");
    this->Eat(TokenType::AS, "expected as after identifier in alias declaration");

    target = this->ParseScope();
    end = target->end;

    return std::make_unique<Alias>(std::move(name), std::move(name), pub, start, end);
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
    variable = std::make_unique<Variable>(this->currTk_.value, pub, start);
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

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after let keyword");
    this->Eat(TokenType::EQUAL, "expected = after identifier in let declaration");
    return std::make_unique<Constant>(name, this->TestList(), pub, start);
}

ast::NodeUptr Parser::FuncDecl(bool pub) {
    Pos start = this->currTk_.start;
    std::string name;
    std::list<NodeUptr> params;
    auto dpos = this->BeginDocs();

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after func keyword");
    if (this->Match(TokenType::LEFT_ROUND)) {
        this->Eat();
        params = this->Param();
        this->Eat(TokenType::RIGHT_ROUND, "expected ) after function params");
    }

    auto fn = std::make_unique<Function>(name, std::move(params), this->Block(), pub, start);
    fn->docs = this->GetDocs(dpos);
    return fn;
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
        id = std::make_unique<Identifier>(this->currTk_, true);
        id->start = start;
        this->Eat();
    }

    return id;
}

ast::NodeUptr Parser::StructDecl(bool pub) {
    Pos start = this->currTk_.start;
    std::string name;
    std::list<NodeUptr> impls;
    auto dpos = this->BeginDocs();

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after struct keyword");

    if (this->Match(TokenType::IMPL)) {
        this->Eat();
        impls = this->TraitList();
    }

    auto structure = std::make_unique<Construct>(NodeType::STRUCT, name, impls, this->StructBlock(), pub, start);
    structure->docs = this->GetDocs(dpos);
    return structure;
}

ast::NodeUptr Parser::StructBlock() {
    auto block = std::make_unique<ast::Block>(NodeType::BLOCK, this->currTk_.start);
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
    auto dpos = this->BeginDocs();

    this->Eat();
    name = this->currTk_.value;
    this->Eat(TokenType::IDENTIFIER, "expected identifier after trait keyword");

    if (this->Match(TokenType::COLON)) {
        this->Eat();
        impls = this->TraitList();
    }

    auto trait = std::make_unique<Construct>(NodeType::TRAIT, name, impls, this->TraitBlock(), pub, start);
    trait->docs = this->GetDocs(dpos);
    return trait;
}

ast::NodeUptr Parser::TraitBlock() {
    auto block = std::make_unique<ast::Block>(NodeType::BLOCK, this->currTk_.start);
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
    NodeUptr label;
    NodeUptr tmp;

    if (this->Match(TokenType::IDENTIFIER) && this->scanner_->Peek().type == TokenType::COLON) {
        label = std::make_unique<Identifier>(this->currTk_);
        this->Eat();
        this->MatchEat(TokenType::COLON, true);
    }

    if (this->TokenInRange(TokenType::KEYWORD_BEGIN, TokenType::KEYWORD_END)) {
        switch (this->currTk_.type) {
            case TokenType::DEFER:
                this->Eat();
                tmp = std::make_unique<Unary>(NodeType::DEFER, this->AtomExpr(), start);
                break;
            case TokenType::SPAWN:
                this->Eat();
                tmp = std::make_unique<Unary>(NodeType::SPAWN, this->AtomExpr(), start);
                break;
            case TokenType::RETURN:
                this->Eat();
                tmp = std::make_unique<Unary>(NodeType::RETURN, this->TestList(), start);
                break;
            case TokenType::IMPORT:
                tmp = this->ImportStmt();
                break;
            case TokenType::FROM:
                tmp = this->FromImportStmt();
                break;
            case TokenType::FOR:
                tmp = this->ForStmt();
                break;
            case TokenType::LOOP:
                tmp = this->LoopStmt();
                break;
            case TokenType::IF:
                tmp = this->IfStmt();
                break;
            case TokenType::SWITCH:
                tmp = this->SwitchStmt();
                break;
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::FALLTHROUGH:
            case TokenType::GOTO:
                tmp = this->JmpStmt();
                break;
            case TokenType::FALSE:
            case TokenType::NIL:
            case TokenType::TRUE:
                tmp = this->Expression();
                break;
            default:
                throw SyntaxException("expected statement", this->currTk_);
        }
    } else
        tmp = this->Expression();

    if (label)
        return std::make_unique<Binary>(NodeType::LABEL, std::move(label), std::move(tmp));

    return tmp;
}

ast::NodeUptr Parser::ImportStmt() {
    auto import = std::make_unique<Import>(this->currTk_.start);

    this->Eat(TokenType::IMPORT, "expected import keyword");

    import->AddName(this->ScopeAsName(false));

    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        import->AddName(this->ScopeAsName(false));
    }

    return import;
}

ast::NodeUptr Parser::FromImportStmt() {
    Pos start = this->currTk_.start;
    std::unique_ptr<Import> import;

    this->Eat(TokenType::FROM, "expected from keyword");

    import = std::make_unique<Import>(this->ScopeAsName(false), start);

    this->Eat(TokenType::IMPORT, "expected import keyword");

    import->AddName(this->ScopeAsName(true));
    while (this->Match(TokenType::COMMA)) {
        this->Eat();
        import->AddName(this->ScopeAsName(true));
    }

    return import;
}

ast::NodeUptr Parser::ScopeAsName(bool id_only) {
    Pos start = this->currTk_.start;
    Pos end;
    NodeUptr path;
    NodeUptr alias;

    path = this->ParseScope();
    end = path->end;

    if (id_only && path->type != NodeType::IDENTIFIER)
        throw SyntaxException("expected name", this->currTk_);

    if (this->Match(TokenType::AS)) {
        this->Eat();
        alias = std::make_unique<Identifier>(this->currTk_);
        end = alias->end;
        this->Eat(TokenType::IDENTIFIER, "expected alias name");
    }

    return std::make_unique<Alias>(std::move(alias), std::move(path), start, end);
}

ast::NodeUptr Parser::ForStmt() {
    Pos start = this->currTk_.start;
    NodeType type = NodeType::FOR;
    NodeUptr init;
    NodeUptr test;
    NodeUptr inc;

    this->Eat();

    if (!this->Match(TokenType::SEMICOLON)) {
        if (this->Match(TokenType::VAR))
            init = this->VarDecl(false);
        else
            init = this->Expression();
    }

    if (this->Match(TokenType::IN)) {
        if (init->type != NodeType::IDENTIFIER && init->type != NodeType::TUPLE)
            throw SyntaxException("expected identifier or tuple", this->currTk_);

        if (init->type == NodeType::TUPLE) {
            for (auto &item : CastNode<List>(init)->expressions) {
                if (item->type != NodeType::IDENTIFIER)
                    throw SyntaxException("expected identifiers list", this->currTk_);
            }
        }
        this->Eat();
        type = NodeType::FOR_IN;
    }

    if (type == NodeType::FOR) {
        this->Eat(TokenType::SEMICOLON, "expected ; after init");
        test = this->Test();
        this->Eat(TokenType::SEMICOLON, "expected ; after test condition");
        inc = this->Expression();
    } else
        test = this->Expression();

    return std::make_unique<For>(type, std::move(init), std::move(test), std::move(inc), this->Block(), start);
}

ast::NodeUptr Parser::LoopStmt() {
    Pos start = this->currTk_.start;
    NodeUptr test;

    this->Eat();

    if (this->Match(TokenType::LEFT_BRACES))
        return std::make_unique<Loop>(this->Block(), start);

    test = this->Test();
    return std::make_unique<Loop>(std::move(test), this->Block(), start);
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
    TokenType kind = this->currTk_.type;

    if (this->TokenInRange(TokenType::ASSIGNMENT_BEGIN, TokenType::ASSIGNMENT_END)) {
        this->Eat();
        return std::make_unique<Assignment>(kind, std::move(left), this->TestList());
    }

    return left;
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
    Pos end;

    if (this->Match(TokenType::EXCLAMATION_LBRACES))
        left = this->Initializer(std::move(left));

    end = left->end;

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

ast::NodeUptr Parser::Initializer(NodeUptr left) {
    auto init = std::make_unique<StructInit>(std::move(left));
    NodeUptr key;

    this->Eat();

    if (!this->MatchEatNL(TokenType::RIGHT_BRACES)) {
        key = this->Test();
        if (this->MatchEatNL(TokenType::COMMA)) {
            init->AddArgument(std::move(key));
            while (this->MatchEat(TokenType::COMMA, true))
                init->AddArgument(this->Test());
        } else if (this->MatchEatNL(TokenType::COLON)) {
            do {
                if (!key) key = this->Test();
                if (key->type != NodeType::IDENTIFIER)
                    throw SyntaxException("expected identifier as key", this->currTk_);
                this->EatTerm(false, TokenType::SEMICOLON);
                this->Eat(TokenType::COLON, "missing value after key in struct initialization");
                this->EatTerm(false, TokenType::SEMICOLON);
                init->AddKeyValue(std::move(key), this->Test());
            } while (this->MatchEat(TokenType::COMMA, true));
        }
    }

    init->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_BRACES, "expected } after struct initialization");

    return init;
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
        case TokenType::SELF:
            tmp = std::make_unique<Identifier>(this->currTk_);
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

    if (!this->MatchEatNL(TokenType::RIGHT_ROUND)) {
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
        auto dpos = this->BeginDocs();
        auto fn = std::make_unique<Function>(std::move(params), this->Block(), start);
        fn->docs = this->GetDocs(dpos);
        return fn;
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

    if (!tmp) {
        params.push_back(this->Test());
        while (this->MatchEat(TokenType::COMMA, true)) {
            if ((tmp = this->Variadic()) != nullptr) {
                params.push_back(std::move(tmp));
                break;
            }
            params.push_back(this->Test());
        }
    } else
        params.push_back(std::move(tmp));

    return params;
}

ast::NodeUptr Parser::ParseList() {
    auto list = std::make_unique<List>(NodeType::LIST, this->currTk_.start);

    this->Eat();

    if (!this->MatchEatNL(TokenType::RIGHT_SQUARE)) {
        list->AddExpression(this->Test());

        while (this->MatchEat(TokenType::COMMA, true))
            list->AddExpression(this->Test());
    }

    list->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_SQUARE, "expected ] after list definition");

    return list;
}

ast::NodeUptr Parser::ParseMapOrSet() {
    auto ms = std::make_unique<List>(NodeType::MAP, this->currTk_.start);
    NodeUptr key;

    this->Eat();

    if (!this->MatchEatNL(TokenType::RIGHT_BRACES)) {
        ms->type = NodeType::SET;
        key = this->Test();
        if (this->MatchEatNL(TokenType::COMMA)) {
            ms->AddExpression(std::move(key));
            while (this->MatchEat(TokenType::COMMA, true))
                ms->AddExpression(this->Test());
        } else if (this->MatchEatNL(TokenType::COLON)) {
            ms->type = NodeType::MAP;
            do {
                if (!key) key = this->Test();
                ms->AddExpression(std::move(key));
                this->EatTerm(false, TokenType::SEMICOLON);
                this->Eat(TokenType::COLON, "missing value after key in map definition");
                this->EatTerm(false, TokenType::SEMICOLON);
                ms->AddExpression(this->Test());
            } while (this->MatchEat(TokenType::COMMA, true));
        }
    }

    ms->end = this->currTk_.end;
    this->Eat(TokenType::RIGHT_BRACES, "expected } after map/set definition");
    return ms;
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

std::list<Comment>::iterator Parser::BeginDocs() {
    if (this->comments_.empty())
        return this->comments_.end();
    return --this->comments_.end();
}

std::list<Comment> Parser::GetDocs(std::list<Comment>::iterator &pos) {
    std::list<Comment> docs;

    if (pos == this->comments_.end())
        docs.splice(docs.begin(), this->comments_, this->comments_.begin(), this->comments_.end());
    else
        docs.splice(docs.begin(), this->comments_, ++pos, this->comments_.end());

    return docs;
}

bool Parser::MatchEat(scanner::TokenType type, bool eat_nl) {
    if (eat_nl)
        this->EatTerm(false, scanner::TokenType::SEMICOLON);

    if (this->currTk_.type == type) {
        this->Eat();
        if (eat_nl)
            this->EatTerm(false, scanner::TokenType::SEMICOLON);
        return true;
    }

    return false;
}

inline bool Parser::MatchEatNL(scanner::TokenType type) {
    this->EatTerm(false, scanner::TokenType::SEMICOLON);
    return this->Match(type);
}

inline bool Parser::TokenInRange(scanner::TokenType begin, scanner::TokenType end) {
    return this->currTk_.type > begin && this->currTk_.type < end;
}

void Parser::Eat() {
    this->currTk_ = this->scanner_->Next();

    while (this->Match(TokenType::INLINE_COMMENT, TokenType::COMMENT)) {
        // Ignore inline comment, BUT KEEP multi-line comment, are very useful to produce documentation!
        if (this->Match(TokenType::COMMENT))
            this->comments_.emplace_back(this->currTk_);
        this->currTk_ = this->scanner_->Next();
    }
}

inline void Parser::Eat(TokenType type, std::string errmsg) {
    if (!this->Match(type)) throw SyntaxException(std::move(errmsg), this->currTk_);
    this->Eat();
}

inline void Parser::EatTerm(bool must_eat) {
    this->EatTerm(must_eat, TokenType::END_OF_FILE);
}

void Parser::EatTerm(bool must_eat, TokenType stop_token) {
    if (!this->Match(stop_token)) {
        while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON)) {
            this->Eat();
            must_eat = false;
        }

        if (must_eat)
            throw SyntaxException("expected EOL or SEMICOLON", this->currTk_);
    }
}
