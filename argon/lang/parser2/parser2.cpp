// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/stringbuilder.h>

#include <argon/lang/parser2/parser2.h>
#include "argon/vm/datatype/nil.h"

using namespace argon::lang::scanner;
using namespace argon::lang::parser2;
using namespace argon::lang::parser2::node;

#define TKCUR_LOC   this->tkcur_.loc
#define TKCUR_TYPE  this->tkcur_.type
#define TKCUR_START this->tkcur_.loc.start
#define TKCUR_END   this->tkcur_.loc.end

bool Parser::CheckIDExt() {
    return this->Match(scanner::TokenType::IDENTIFIER);
}

List *Parser::ParseFnParams() {
    ARC ret;

    Position start{};

    auto *params = (List *) ListNew();
    if (params == nullptr)
        throw DatatypeException();

    ret = params;

    ArObject *tmp;

    int mode = 0;

    do {
        if (this->Match(TokenType::RIGHT_ROUND))
            break;

        if (this->Match(TokenType::AMPERSAND)) {
            if (mode > 2)
                throw ParserException(TKCUR_LOC, kStandardError[10]);

            mode = 3;

            start = this->tkcur_.loc.start;

            this->Eat(false);

            if (!this->CheckIDExt())
                throw ParserException(TKCUR_LOC, kStandardError[4], "&");

            tmp = (ArObject *) this->ParseFuncParam(start, NodeType::KWPARAM);
        } else if (this->Match(TokenType::ELLIPSIS)) {
            if (mode > 1)
                throw ParserException(TKCUR_LOC, kStandardError[11]);

            mode = 2;

            start = TKCUR_START;

            this->Eat(false);

            if (!this->CheckIDExt())
                throw ParserException(TKCUR_LOC, kStandardError[4], "...");

            tmp = (ArObject *) this->ParseFuncParam(start, NodeType::REST);
        } else {
            if (mode > 1)
                throw ParserException(TKCUR_LOC, kStandardError[12]);

            tmp = (ArObject *) this->ParseFuncNameParam(false);

            if (mode > 0 && ((Parameter *) tmp)->value == nullptr) {
                Release(tmp);

                throw ParserException(((Parameter *) tmp)->loc, "unexpected non keyword parameter");
            }

            if (((Parameter *) tmp)->node_type == NodeType::PARAMETER && ((Parameter *) tmp)->value != nullptr)
                mode = 1;
        }

        if (!ListAppend(params, tmp)) {
            Release(tmp);

            throw DatatypeException();
        }

        Release(tmp);
    } while (this->MatchEat(TokenType::COMMA, true));

    ret.Unwrap();

    return params;
}

Node *Parser::ParseAsync(Context *context, Position &start, bool pub) {
    this->Eat(true);

    auto *func = (Function *) this->ParseFunc(context, start, pub);
    func->async = true;

    return (Node *) func;
}

Node *Parser::ParseBlock(Context *context) {
    ARC stmts;

    Position start = TKCUR_START;

    this->EatNL();

    if (!this->Match(TokenType::LEFT_BRACES))
        throw ParserException(TKCUR_LOC, kStandardError[7]);

    List *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    stmts = list;

    // +++ Parse doc strings
    if (context->type == ContextType::FUNC
        || context->type == ContextType::STRUCT
        || context->type == ContextType::TRAIT)
        context->doc = this->ParseDoc();
    else
        this->Eat(true);
    // ---

    while (!this->Match(TokenType::RIGHT_BRACES)) {
        auto *stmt = this->ParseDecls(context);

        if (!ListAppend(list, (ArObject *) stmt)) {
            Release(stmt);

            throw DatatypeException();
        }

        Release(stmt);

        this->EatNL();
    }

    auto *block = NewNode<Unary>(type_ast_unary_, false, NodeType::BLOCK);
    if (block == nullptr)
        throw DatatypeException();

    block->loc.start = start;
    block->loc.end = TKCUR_END;

    block->value = (ArObject *) list;

    stmts.Unwrap();

    this->Eat(false);

    return (Node *) block;
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
            decl = this->ParseAsync(context, start, pub);
            break;
        case TokenType::KW_FROM:
            break;
        case TokenType::KW_FUNC:
            decl = this->ParseFunc(context, start, pub);
            break;
        case TokenType::KW_IMPORT:
            break;
        case TokenType::KW_LET:
            decl = this->ParseVarDecl(context, start, true, pub, false);
            break;
        case TokenType::KW_VAR:
            if (Parser::CheckScope(context, ContextType::TRAIT))
                throw ParserException(TKCUR_LOC, kStandardError[1]);

            decl = this->ParseVarDecl(context, start, false, pub, false);
            break;
        case TokenType::KW_STRUCT:
            break;
        case TokenType::KW_SYNC:
            if (Parser::CheckScope(context, ContextType::STRUCT, ContextType::TRAIT))
                throw ParserException(TKCUR_LOC, kStandardError[13], "sync", kContextName[(int) context->type]);

            decl = this->ParseSyncBlock(context);
            break;
        case TokenType::KW_TRAIT:
            break;
        case TokenType::KW_WEAK:
            if (!Parser::CheckScope(context, ContextType::STRUCT))
                throw ParserException(TKCUR_LOC, kStandardError[2]);

            this->Eat(true);

            if (!this->MatchEat(TokenType::KW_VAR, true))
                throw ParserException(TKCUR_LOC, kStandardError[3]);

            decl = this->ParseVarDecl(context, start, false, pub, true);
            break;
        default:
            break;
    }

    return (Node *) decl.Unwrap();
}

Node *Parser::ParseFunc(Context *context, Position start, bool pub) {
    ARC name;
    ARC params;
    ARC body;

    Context f_ctx(context, ContextType::FUNC);

    this->Eat(true);

    if (!this->CheckIDExt())
        throw ParserException(TKCUR_LOC, kStandardError[4], "func");

    name = Parser::ParseIdentifierSimple(&this->tkcur_);

    this->Eat(true);

    if (this->MatchEat(TokenType::LEFT_ROUND, true)) {
        params = this->ParseFnParams();

        this->EatNL();

        if (!this->MatchEat(TokenType::RIGHT_ROUND, true))
            throw ParserException(TKCUR_LOC, kStandardError[6]);
    }

    if (!CheckScope(context, ContextType::TRAIT) || this->Match(scanner::TokenType::LEFT_BRACES))
        body = this->ParseBlock(&f_ctx);

    auto *func = NewNode<Function>(type_ast_function_, false, NodeType::FUNCTION);
    if (func == nullptr)
        throw DatatypeException();

    func->loc.start = start;

    func->name = (String *) name.Unwrap();
    func->doc = context->doc;

    func->params = (List *) params.Unwrap();
    func->body = (Node *) body.Unwrap();

    if (func->body != nullptr)
        func->loc.end = func->body->loc.end;
    else
        func->loc.end = TKCUR_START;

    func->async = false;
    func->pub = pub;

    return (Node *) func;
}

node::Node *Parser::ParseFuncNameParam(bool parse_pexpr) {
    ARC id;
    ARC value;

    Loc loc{};

    bool identifier = false;

    if (parse_pexpr) {
        // TODO: id = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::COMMA));
    } else if (this->CheckIDExt()) {
        id = Parser::ParseIdentifierSimple(&this->tkcur_);
        identifier = true;

        this->Eat(true);
    } else
        throw ParserException(TKCUR_LOC, kStandardError[9]);

    this->EatNL();

    loc = TKCUR_LOC;

    if (this->MatchEat(TokenType::EQUAL, true)) {
        if (!identifier)
            throw ParserException(loc, kStandardError[8]);

        if (this->Match(TokenType::COMMA, TokenType::RIGHT_ROUND)) {
            auto *literal = NewNode<Unary>(type_ast_literal_, false, NodeType::LITERAL);
            if (literal == nullptr)
                throw DatatypeException();

            literal->loc = loc;

            value = literal;
        } else {
            // TODO: (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::COMMA));
        }
    }

    if (identifier) {
        auto *param = NewNode<Parameter>(type_ast_parameter_, false, NodeType::PARAMETER);
        if (param == nullptr)
            throw DatatypeException();

        param->loc = loc;

        param->id = (String *) id.Unwrap();
        param->value = (Node *) value.Unwrap();

        return (Node *) param;
    }

    return (Node *) id.Unwrap();
}

Node *Parser::ParseFuncParam(Position start, NodeType type) {
    auto *id = Parser::ParseIdentifierSimple(&this->tkcur_);

    assert(type == NodeType::REST || type == NodeType::KWPARAM);

    auto *parameter = NewNode<Parameter>(type_ast_parameter_, false, type);
    if (parameter == nullptr) {
        Release(id);

        throw DatatypeException();
    }

    parameter->loc.start = start;
    parameter->loc.end = TKCUR_END;

    parameter->id = id;

    this->Eat(true);

    return (Node *) parameter;
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

node::Node *Parser::ParseSyncBlock(Context *context) {
    ARC expr;
    ARC body;

    Position start = TKCUR_START;

    this->Eat(true);

    // TODO: expr = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::ASTERISK));

    if (((Node *) expr.Get())->node_type == NodeType::LITERAL)
        throw ParserException(((Node *) expr.Get())->loc, kStandardError[14]);

    body = this->ParseBlock(context);

    auto *sync = NewNode<Binary>(type_ast_sync_, false, NodeType::SYNC_BLOCK);
    if (sync == nullptr)
        throw DatatypeException();

    sync->left = expr.Unwrap();
    sync->right = body.Unwrap();

    sync->loc.start = start;
    sync->loc.end = ((Node *) (sync->right))->loc.end;

    return (Node *) sync;
}

node::Node *Parser::ParseVarDecl(Context *context, Position start, bool constant, bool pub, bool weak) {
    ARC ret;
    Token identifier;

    Assignment *assignment;

    this->Eat(true);

    if (!this->CheckIDExt())
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

    auto *id = (ArObject *) Parser::ParseIdentifierSimple(&token);
    if (!ListAppend(ids, id)) {
        Release(id);

        throw DatatypeException();
    }

    Release(id);

    do {
        this->EatNL();

        id = (ArObject *) Parser::ParseIdentifierSimple(&this->tkcur_);
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

String *Parser::ParseDoc() {
    StringBuilder builder{};

    do {
        if (!this->scanner_.NextToken(&this->tkcur_))
            assert(false);
    } while (this->Match(TokenType::END_OF_LINE));

    while (this->TokenInRange(TokenType::COMMENT_BEGIN, TokenType::COMMENT_END)) {
        if (!builder.Write(this->tkcur_.buffer, this->tkcur_.length, 0))
            throw DatatypeException();

        do {
            if (!this->scanner_.NextToken(&this->tkcur_))
                assert(false);
        } while (this->Match(TokenType::END_OF_LINE));
    }

    auto *ret = builder.BuildString();
    if (ret == nullptr)
        throw DatatypeException();

    return ret;
}

String *Parser::ParseIdentifierSimple(const scanner::Token *token) {
    auto *id = StringNew((const char *) token->buffer, token->length);
    if (id == nullptr)
        throw DatatypeException();

    return id;
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