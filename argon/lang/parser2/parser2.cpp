// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/parser2.h>

using namespace argon::lang::scanner;
using namespace argon::lang::parser2;
using namespace argon::lang::parser2::node;

#define TKCUR_LOC this->tkcur_.loc
#define TKCUR_TYPE this->tkcur_.type
#define TKCUR_START this->tkcur_.loc.start
#define TKCUR_END this->tkcur_.loc.end

ArObject *Parser::ParseIdentifierSimple(const scanner::Token *token) {
    auto *id = StringNew((const char *) token->buffer, token->length);
    if (id == nullptr)
        throw DatatypeException();

    return (ArObject *) id;
}

Node *Parser::ParseDecls(Context *context) {
    ARC decl;
    Position start = TKCUR_START;

    bool pub = false;
    if (this->MatchEat(TokenType::KW_PUB, true)) {
        pub = true;

        if (!this->CheckScope(context, ContextType::MODULE, ContextType::STRUCT, ContextType::TRAIT))
            throw ParserException(TKCUR_LOC,
                                  "'pub' modifier not allowed in %s",
                                  kContextName[(int) context->type]);
    }

    this->EatNL();

    switch (TKCUR_TYPE) {
        case TokenType::KW_ASYNC:
        case TokenType::KW_FROM:
        case TokenType::KW_FUNC:
        case TokenType::KW_IMPORT:
            break;
        case TokenType::KW_LET:
            decl = this->ParseVarDecl(context, start, true, pub, false);
            break;
        case TokenType::KW_VAR:
            if (this->CheckScope(context, ContextType::TRAIT))
                throw ParserException(TKCUR_LOC, kStandardError[1]);

            decl = this->ParseVarDecl(context, start, false, pub, false);
            break;
        case TokenType::KW_STRUCT:
        case TokenType::KW_SYNC:
        case TokenType::KW_TRAIT:
        case TokenType::KW_WEAK:
            if (!this->CheckScope(context, ContextType::STRUCT))
                throw ParserException(TKCUR_LOC, kStandardError[2]);

            this->Eat(true);

            if (!this->MatchEat(TokenType::KW_VAR, true))
                throw ParserException(TKCUR_LOC, kStandardError[3]);

            decl = this->ParseVarDecl(context, start, false, pub, true);
            break;
        default:
            break;
    }

    return nullptr;
}

node::Node *Parser::ParseIdentifier(scanner::Token *token) {
    auto *id = StringNew((const char *) token->buffer, token->length);
    if (id == nullptr)
        throw DatatypeException();

    auto *node = NewNode<Unary>(type_ast_identifier_, false, NodeType::IDENTIFIER);
    if (node == nullptr) {
        Release(id);

        throw DatatypeException();
    }

    node->value = (ArObject *) id;

    return (Node *) node;
}

node::Node *Parser::ParseVarDecl(Context *context, Position start, bool constant, bool pub, bool weak) {
    ARC ret;
    Token identifier;

    Assignment *assignment;

    this->Eat(true);

    if (!this->Match(TokenType::IDENTIFIER))
        throw ParserException(TKCUR_LOC, kStandardError[4], constant ? "let" : "var");

    identifier = std::move(this->tkcur_);

    this->Eat(false);
    this->IgnoreNewLineIF(TokenType::COMMA, TokenType::EQUAL);

    if (this->Match(TokenType::EQUAL)) {
        auto *id = Parser::ParseIdentifierSimple(&identifier);

        if ((assignment = NewNode<Assignment>(type_ast_assignment_, false, NodeType::ASSIGNMENT)) == nullptr) {
            Release(id);

            throw DatatypeException();
        }

        assignment->name = (ArObject *) id;
    } else if (this->Match(TokenType::COMMA)) {
        this->Eat(true);
        assignment = (Assignment *) this->ParseVarDecls(identifier);
    } else
        throw ParserException(TKCUR_LOC, kStandardError[0]);

    ret = assignment;

    this->IgnoreNewLineIF(TokenType::EQUAL);

    if (this->MatchEat(TokenType::EQUAL, false)) {
        this->EatNL();

        // TODO: auto *values = this->ParseExpression(PeekPrecedence(TokenType::EQUAL));
        // TODO: assignment->loc.end = values->loc.end;
        // TODO: assignment->value = (ArObject *) values;
    } else if (constant)
        throw ParserException(TKCUR_LOC, kStandardError[5]);

    assignment->loc.start = start;

    assignment->constant = constant;
    assignment->pub = pub;
    assignment->weak = weak;

    return (Node *) ret.Unwrap();
}

node::Node *Parser::ParseVarDecls(const Token &token) {
    ARC list;

    Position end{};

    List *ids = ListNew();
    if (ids == nullptr)
        throw DatatypeException();

    auto *id = Parser::ParseIdentifierSimple(&token);
    if (!ListAppend(ids, id)) {
        Release(id);

        throw DatatypeException();
    }

    Release(id);

    do {
        this->EatNL();

        id = Parser::ParseIdentifierSimple(&this->tkcur_);
        if (!ListAppend(ids, id)) {
            Release(id);

            throw DatatypeException();
        }
        Release(id);

        end = TKCUR_END;

        this->Eat(false);
        this->IgnoreNewLineIF(scanner::TokenType::COMMA);
    } while (this->MatchEat(TokenType::COMMA, false));

    auto *assignment = NewNode<Assignment>(type_ast_assignment_, false, NodeType::ASSIGNMENT);
    if (assignment == nullptr)
        throw DatatypeException();

    list.Unwrap();

    assignment->loc.end = end;

    assignment->multi = true;
    assignment->name = (ArObject *) ids;

    return (Node *) assignment;
}

void Parser::Eat(bool ignore_nl) {
    if (this->tkcur_.type == TokenType::END_OF_FILE)
        return;

    do {
        if (!this->scanner_.NextToken(&this->tkcur_))
            return;

        if (ignore_nl) {
            while (this->tkcur_.type == TokenType::END_OF_LINE) {
                if (!this->scanner_.NextToken(&this->tkcur_))
                    return;
            }
        }
    } while (this->TokenInRange(scanner::TokenType::COMMENT_BEGIN, scanner::TokenType::COMMENT_END));
}

void Parser::EatNL() {
    if (this->tkcur_.type == TokenType::END_OF_LINE)
        this->Eat(true);
}

Module *Parser::Parse() {
    ARC ret;
    ARC statements;

    auto *module = NewNode<Module>(type_ast_module_, false, NodeType::MODULE);
    if (module == nullptr)
        return nullptr;

    ret = module;

    if ((module->filename = StringNew(this->filename_)) == nullptr)
        return nullptr;

    auto *stmts = ListNew();
    if (stmts == nullptr)
        return nullptr;

    statements = stmts;

    try {
        Context context(ContextType::MODULE);

        // Init
        this->Eat(true);

        this->ParseDecls(&context);
    } catch (ParserException &e) {
        assert(e.what() != nullptr);
    }

    module->docs = nullptr;
    module->statements = stmts;

    statements.Unwrap();

    return (Module *) ret.Unwrap();
}