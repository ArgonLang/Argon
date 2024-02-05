// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bytes.h>
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

int Parser::PeekPrecedence(TokenType type) {
    switch (type) {
        case TokenType::END_OF_LINE:
        case TokenType::END_OF_FILE:
            return -1;
        default:
            return 1000;
    }

    assert(false);
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

List *Parser::ParseTraitList() {
    ARC ret;

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    ret = list;

    do {
        auto *scope = this->ParseScope();

        if (!ListAppend(list, (ArObject *) scope)) {
            Release(scope);

            throw DatatypeException();
        }

        Release(scope);
    } while (this->MatchEat(TokenType::COMMA, true));

    return (List *) ret.Unwrap();
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
            if (!Parser::CheckScope(context, ContextType::MODULE))
                throw ParserException(TKCUR_LOC, kStandardError[13], "from", kContextName[(int) context->type]);

            decl = this->ParseFromImport(pub);
            break;
        case TokenType::KW_FUNC:
            decl = this->ParseFunc(context, start, pub);
            break;
        case TokenType::KW_IMPORT:
            if (!Parser::CheckScope(context, ContextType::MODULE))
                throw ParserException(TKCUR_LOC, kStandardError[13], "import", kContextName[(int) context->type]);

            decl = this->ParseImport(pub);
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
            if (!Parser::CheckScope(context, ContextType::MODULE))
                throw ParserException(TKCUR_LOC, kStandardError[13], "struct", kContextName[(int) context->type]);

            decl = this->ParseStruct(context, pub);
            break;
        case TokenType::KW_SYNC:
            if (Parser::CheckScope(context, ContextType::STRUCT, ContextType::TRAIT))
                throw ParserException(TKCUR_LOC, kStandardError[13], "sync", kContextName[(int) context->type]);

            decl = this->ParseSyncBlock(context);
            break;
        case TokenType::KW_TRAIT:
            if (!Parser::CheckScope(context, ContextType::MODULE))
                throw ParserException(TKCUR_LOC, kStandardError[13], "trait", kContextName[(int) context->type]);

            decl = this->ParseTrait(context, pub);
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
            if (pub)
                throw ParserException(TKCUR_LOC, kStandardError[17]);

            decl = this->ParseStatement(context);
            break;
    }

    return (Node *) decl.Unwrap();
}

Node *Parser::ParseExpression(Context *context) {
    Node *expr;

    expr = this->ParseExpression(context, 0);

    if (this->Match(TokenType::COLON)) {
        if (expr->node_type != NodeType::IDENTIFIER) {
            Release(expr);

            throw ParserException(TKCUR_LOC, kStandardError[0]);
        }

        return expr;
    }

    // TODO: Safe Expr

    return expr;
}

Node *Parser::ParseFromImport(bool pub) {
    // from "x/y/z" import xyz as x
    ARC imp_list;

    ARC m_name;
    ARC id;
    ARC alias;

    Position start = TKCUR_START;
    Position end{};

    this->Eat(true);

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    imp_list = list;

    if (!this->Match(TokenType::STRING))
        throw ParserException(TKCUR_LOC, kStandardError[15], "from");

    m_name = this->ParseLiteral(nullptr);

    if (!this->MatchEat(TokenType::KW_IMPORT, true))
        throw ParserException(TKCUR_LOC, kStandardError[16]);

    do {
        this->EatNL();

        if (!this->CheckIDExt()) {
            if (this->MatchEat(TokenType::ASTERISK, false)) {
                imp_list.Discard();

                break;
            }

            throw ParserException(TKCUR_LOC, kStandardError[17]);
        }

        id = Parser::ParseIdentifierSimple(&this->tkcur_);

        end = TKCUR_END;

        this->Eat(false);

        this->IgnoreNewLineIF(TokenType::KW_AS);

        if (this->MatchEat(TokenType::KW_AS, false)) {
            this->EatNL();

            if (!this->CheckIDExt())
                throw ParserException(TKCUR_LOC, kStandardError[4], "as");

            alias = Parser::ParseIdentifierSimple(&this->tkcur_);

            end = TKCUR_END;

            this->Eat(false);
        }

        auto *i_name = NewNode<Binary>(type_ast_import_name_, false, NodeType::IMPORT_NAME);
        if (i_name == nullptr)
            throw DatatypeException();

        i_name->left = id.Unwrap();
        i_name->right = alias.Unwrap();

        i_name->loc.start = ((Node *) (i_name->left))->loc.start;
        i_name->loc.end = end;

        if (!ListAppend(list, (ArObject *) i_name)) {
            Release(i_name);

            throw DatatypeException();
        }

        Release(i_name);

        this->IgnoreNewLineIF(TokenType::COMMA);
    } while (this->MatchEat(TokenType::COMMA, false));

    auto *imp = NewNode<Import>(type_ast_import_, false, NodeType::IMPORT);
    if (imp == nullptr)
        throw DatatypeException();

    imp->loc.start = start;
    imp->loc.end = end;

    imp->mod = (Node *) m_name.Unwrap();
    imp->names = imp_list.Unwrap();

    imp->pub = pub;

    return (Node *) imp;
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
    func->doc = IncRef(context->doc);

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

Node *Parser::ParseImport(bool pub) {
    ARC path;
    ARC ret;

    Position start = TKCUR_START;
    Position end{};

    this->Eat(true);

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    ret = list;

    do {
        String *id = nullptr;

        this->EatNL();

        if (!this->Match(TokenType::STRING))
            throw ParserException(TKCUR_LOC, kStandardError[15], "import");

        end = TKCUR_END;

        path = this->ParseLiteral(nullptr);

        this->IgnoreNewLineIF(TokenType::KW_AS);

        if (this->MatchEat(scanner::TokenType::KW_AS, false)) {
            this->EatNL();

            if (!this->CheckIDExt())
                throw ParserException(TKCUR_LOC, kStandardError[4], "as");

            id = Parser::ParseIdentifierSimple(&this->tkcur_);

            end = TKCUR_END;

            this->Eat(false);
        }

        auto *i_name = NewNode<Binary>(type_ast_import_name_, false, NodeType::IMPORT_NAME);
        if (i_name == nullptr) {
            Release(id);

            throw DatatypeException();
        }

        i_name->left = path.Unwrap();
        i_name->right = (ArObject *) id;

        i_name->loc.start = ((Node *) (i_name->left))->loc.start;
        i_name->loc.end = end;

        if (!ListAppend(list, (ArObject *) i_name)) {
            Release(i_name);

            throw DatatypeException();
        }

        Release(i_name);

        this->IgnoreNewLineIF(TokenType::COMMA);
    } while (this->MatchEat(TokenType::COMMA, false));

    auto *imp = NewNode<Import>(type_ast_import_, false, NodeType::IMPORT);
    if (imp == nullptr)
        throw DatatypeException();

    imp->loc.start = start;
    imp->loc.end = end;

    imp->names = ret.Unwrap();

    imp->pub = pub;

    return (Node *) imp;
}

node::Node *Parser::ParseLiteral(Context *context) {
    ArObject *value;
    switch (this->tkcur_.type) {
        case TokenType::ATOM:
            value = (ArObject *) AtomNew((const char *) this->tkcur_.buffer);
            break;
        case TokenType::NUMBER:
            value = (ArObject *) IntNew((const char *) this->tkcur_.buffer, 10);
            break;
        case TokenType::U_NUMBER:
            value = (ArObject *) UIntNew((const char *) this->tkcur_.buffer, 10);
            break;
        case TokenType::NUMBER_BIN:
            value = (ArObject *) IntNew((const char *) this->tkcur_.buffer, 2);
            break;
        case TokenType::U_NUMBER_BIN:
            value = (ArObject *) UIntNew((const char *) this->tkcur_.buffer, 2);
            break;
        case TokenType::NUMBER_OCT:
            value = (ArObject *) IntNew((const char *) this->tkcur_.buffer, 8);
            break;
        case TokenType::U_NUMBER_OCT:
            value = (ArObject *) UIntNew((const char *) this->tkcur_.buffer, 8);
            break;
        case TokenType::NUMBER_HEX:
            value = (ArObject *) IntNew((const char *) this->tkcur_.buffer, 16);
            break;
        case TokenType::U_NUMBER_HEX:
            value = (ArObject *) UIntNew((const char *) this->tkcur_.buffer, 16);
            break;
        case TokenType::DECIMAL:
            value = (ArObject *) DecimalNew((const char *) this->tkcur_.buffer);
            break;
        case TokenType::NUMBER_CHR:
            value = (ArObject *) UIntNew(StringUTF8ToInt((const unsigned char *) this->tkcur_.buffer));
            break;
        case TokenType::STRING:
        case TokenType::RAW_STRING:
            value = (ArObject *) StringNew((const char *) this->tkcur_.buffer, this->tkcur_.length);
            break;
        case TokenType::BYTE_STRING:
            value = (ArObject *) BytesNew(this->tkcur_.buffer, this->tkcur_.length, true);
            break;
        case TokenType::FALSE:
            value = (ArObject *) IncRef(False);
            break;
        case TokenType::TRUE:
            value = (ArObject *) IncRef(True);
            break;
        case TokenType::NIL:
            value = (ArObject *) IncRef(Nil);
            break;
        default:
            assert(false); // Never get here!
    }

    if (value == nullptr)
        throw DatatypeException();

    auto *literal = NewNode<Unary>(type_ast_literal_, false, NodeType::LITERAL);
    if (literal == nullptr) {
        Release(value);

        throw DatatypeException();
    }

    literal->loc = TKCUR_LOC;

    literal->value = value;

    this->Eat(false);

    return (Node *) literal;
}

Node *Parser::ParseFuncNameParam(bool parse_pexpr) {
    ARC id;
    ARC value;

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

    Loc loc = TKCUR_LOC;

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

Node *Parser::ParseScope() {
    // TODO: parse selector
    this->Eat(false);
    return nullptr;
}

Node *Parser::ParseStatement(Context *context) {
    ARC expr;
    ARC label;

    do {
        switch (TKCUR_TYPE) {
            // TODO: STATEMENTS
            default:
                expr = this->ParseExpression(context);
                break;
        }

        this->IgnoreNewLineIF(TokenType::COLON);

        if (((Node *) expr.Get())->node_type != NodeType::IDENTIFIER
            || !this->MatchEat(TokenType::COLON, false))
            break;

        this->Eat(true);

        if (label)
            throw ParserException(TKCUR_LOC, kStandardError[19]);

        label = expr;
    } while (true);

    // TODO: label checks

    return (Node *) expr.Unwrap();
}

Node *Parser::ParseStruct(Context *context, bool pub) {
    Context t_ctx(context, ContextType::STRUCT);

    ARC name;
    ARC impls;
    ARC body;

    Position start = TKCUR_START;

    this->Eat(true);

    if (!this->CheckIDExt())
        throw ParserException(TKCUR_LOC, kStandardError[4], "struct");

    name = Parser::ParseIdentifierSimple(&this->tkcur_);

    this->Eat(true);

    if (this->MatchEat(TokenType::KW_IMPL, true))
        impls = this->ParseTraitList();

    body = this->ParseBlock(&t_ctx);

    auto *_struct = NewNode<Construct>(type_ast_struct_, false, NodeType::STRUCT);
    if (_struct == nullptr)
        throw DatatypeException();

    _struct->loc.start = start;
    _struct->loc.end = ((Node *) body.Get())->loc.end;

    _struct->name = (String *) name.Unwrap();
    _struct->doc = IncRef(context->doc);
    _struct->impls = (List *) impls.Unwrap();

    _struct->body = (Node *) body.Unwrap();

    _struct->pub = pub;

    return (Node *) _struct;
}

Node *Parser::ParseSyncBlock(Context *context) {
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

Node *Parser::ParseTrait(Context *context, bool pub) {
    Context t_ctx(context, ContextType::TRAIT);

    ARC name;
    ARC impls;
    ARC body;

    Position start = TKCUR_START;

    this->Eat(true);

    if (!this->CheckIDExt())
        throw ParserException(TKCUR_LOC, kStandardError[4], "trait");

    name = Parser::ParseIdentifierSimple(&this->tkcur_);

    this->Eat(true);

    if (this->MatchEat(TokenType::COLON, true))
        impls = this->ParseTraitList();

    body = this->ParseBlock(&t_ctx);

    auto *trait = NewNode<Construct>(type_ast_trait_, false, NodeType::TRAIT);
    if (trait == nullptr)
        throw DatatypeException();

    trait->loc.start = start;
    trait->loc.end = ((Node *) body.Get())->loc.end;

    trait->name = (String *) name.Unwrap();
    trait->doc = IncRef(context->doc);
    trait->impls = (List *) impls.Unwrap();

    trait->body = (Node *) body.Unwrap();

    trait->pub = pub;

    return (Node *) trait;
}

Node *Parser::ParseVarDecl(Context *context, Position start, bool constant, bool pub, bool weak) {
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

Node *Parser::ParseVarDecls(const Token &token) {
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

// *********************************************************************************************************************
// EXPRESSION-ZONE AFTER THIS POINT
// *********************************************************************************************************************

Parser::LedMeth Parser::LookupLED(TokenType token) {
    if (token > TokenType::INFIX_BEGIN && token < TokenType::INFIX_END)
        return &Parser::ParseInfix;

    switch (token) {
        default:
            return nullptr;
    }

    assert(false);
}

Parser::NudMeth Parser::LookupNUD(TokenType token) {
    if (token > TokenType::LITERAL_BEGIN && token < TokenType::LITERAL_END)
        return &Parser::ParseLiteral;

    switch (token) {
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
            return &Parser::ParsePrefix;
        default:
            return nullptr;
    }

    assert(false);
}

Node *Parser::ParseExpression(Context *context, int precedence) {
    ARC left;

    LedMeth led;
    NudMeth nud;

    if ((nud = Parser::LookupNUD(TKCUR_TYPE)) == nullptr) {
        // As identifier!
        left = this->ParseIdentifier(context);
    } else
        left = (this->*nud)(context);

    auto tk_type = TKCUR_TYPE;
    while (precedence < Parser::PeekPrecedence(tk_type)) {
        if ((led = Parser::LookupLED(tk_type)) == nullptr)
            break;

        left = (this->*led)(context, ((Node *) left.Get()));

        tk_type = TKCUR_TYPE;
    }

    // TODO: safe?!

    return (Node *) left.Unwrap();
}

Node *Parser::ParseIdentifier(Context *context) {
    auto *id = StringNew((const char *) this->tkcur_.buffer, this->tkcur_.length);
    if (id == nullptr)
        throw DatatypeException();

    auto *node = NewNode<Unary>(type_ast_identifier_, false, NodeType::IDENTIFIER);
    if (node == nullptr) {
        Release(id);

        throw DatatypeException();
    }

    node->value = (ArObject *) id;

    this->Eat(false);

    return (Node *) node;
}

Node *Parser::ParseInfix(Context *context, Node *left) {
    TokenType type = TKCUR_TYPE;

    this->Eat(true);

    auto *right = this->ParseExpression(context, Parser::PeekPrecedence(type));

    auto *infix = NewNode<Binary>(type_ast_infix_, false, NodeType::INFIX);
    if (infix == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    infix->loc.start = left->loc.start;
    infix->loc.end = right->loc.end;

    infix->left = (ArObject *) IncRef(left);
    infix->right = (ArObject *) right;

    return (Node *) infix;
}

Node *Parser::ParsePrefix(Context *context) {
    Position start = TKCUR_START;
    TokenType kind = TKCUR_TYPE;

    this->Eat(true);

    auto *right = this->ParseExpression(context, PeekPrecedence(kind));

    auto *unary = NewNode<Unary>(type_ast_unary_, false, NodeType::UNARY);
    if (unary == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    unary->loc.start = start;
    unary->loc.end = unary->loc.end;

    unary->token_type = kind;

    unary->value = (ArObject *) unary;

    return (Node *) unary;
}

// *********************************************************************************************************************
// PUBLIC
// *********************************************************************************************************************

Module *Parser::Parse() {
    ARC ret;
    ARC statements;

    auto *mod = NewNode<Module>(type_ast_module_, false, NodeType::MODULE);
    if (mod == nullptr)
        return nullptr;

    ret = mod;

    if ((mod->filename = StringNew(this->filename_)) == nullptr)
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

    mod->docs = nullptr;
    mod->statements = stmts;

    statements.Unwrap();

    return (Module *) ret.Unwrap();
}