// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/stringbuilder.h>

#include <argon/lang/parser2/parser2.h>

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

List *Parser::ParseFnParams(Context *context, bool parse_pexpr) {
    ARC ret;

    Position start{};

    auto *params = ListNew();
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

            tmp = (ArObject *) this->ParseFuncNameParam(context, parse_pexpr);

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

        // TODO: check NewLine or ;
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
        params = this->ParseFnParams(context, false);

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
    func->doc = IncRef(f_ctx.doc);

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

Node *Parser::ParseFuncNameParam(Context *context, bool parse_pexpr) {
    ARC id;
    ARC value;

    bool identifier = false;

    if (parse_pexpr)
        id = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));
    else if (this->CheckIDExt()) {
        id = Parser::ParseIdentifierSimple(&this->tkcur_);
        identifier = true;

        this->Eat(true);
    } else
        throw ParserException(TKCUR_LOC, kStandardError[9]);

    this->EatNL();

    Loc loc = TKCUR_LOC;

    if (this->MatchEat(TokenType::EQUAL, true)) {
        if (!identifier && ((Node *) id.Get())->node_type != NodeType::IDENTIFIER)
            throw ParserException(loc, kStandardError[8]);

        if (this->Match(TokenType::COMMA, TokenType::RIGHT_ROUND)) {
            auto *literal = NewNode<Unary>(type_ast_literal_, false, NodeType::LITERAL);
            if (literal == nullptr)
                throw DatatypeException();

            literal->loc = loc;

            value = literal;
        } else
            value = this->ParseExpression(context, PeekPrecedence(scanner::TokenType::COMMA));
    }

    if (identifier || value) {
        auto *param = NewNode<Parameter>(type_ast_parameter_, false, NodeType::PARAMETER);
        if (param == nullptr)
            throw DatatypeException();

        param->loc = loc;

        auto *r_id = id.Unwrap();

        if (!AR_TYPEOF(r_id, vm::datatype::type_string_)) {
            param->id = (String *) IncRef(((Unary *) r_id)->value);

            Release(r_id);
        } else
            param->id = (String *) r_id;

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

    Assignment *vardecl;

    this->Eat(true);

    if (!this->CheckIDExt())
        throw ParserException(TKCUR_LOC, kStandardError[4], constant ? "let" : "var");

    identifier = std::move(this->tkcur_);

    this->Eat(false);
    this->IgnoreNewLineIF(TokenType::COMMA, TokenType::EQUAL);

    if (this->Match(TokenType::EQUAL)) {
        auto *id = Parser::ParseIdentifierSimple(&identifier);

        if ((vardecl = NewNode<Assignment>(type_ast_vardecl_, false, NodeType::VARDECL)) == nullptr) {
            Release(id);

            throw DatatypeException();
        }

        vardecl->name = (ArObject *) id;
    } else if (this->Match(TokenType::COMMA)) {
        this->Eat(true);
        vardecl = (Assignment *) this->ParseVarDecls(identifier);
    } else
        throw ParserException(TKCUR_LOC, kStandardError[0]);

    ret = vardecl;

    this->IgnoreNewLineIF(TokenType::EQUAL);

    if (this->MatchEat(TokenType::EQUAL, false)) {
        this->EatNL();

        // TODO: auto *values = this->ParseExpression(PeekPrecedence(TokenType::EQUAL));
        // TODO: vardecl->loc.end = values->loc.end;
        // TODO: vardecl->value = (ArObject *) values;
    } else if (constant)
        throw ParserException(TKCUR_LOC, kStandardError[5]);

    vardecl->loc.start = start;

    vardecl->constant = constant;
    vardecl->pub = pub;
    vardecl->weak = weak;

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

    auto *vardecl = NewNode<Assignment>(type_ast_vardecl_, false, NodeType::VARDECL);
    if (vardecl == nullptr)
        throw DatatypeException();

    list.Unwrap();

    vardecl->loc.end = end;

    vardecl->multi = true;
    vardecl->name = (ArObject *) ids;

    return (Node *) vardecl;
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

Parser::LedMeth Parser::LookupLED(TokenType token, bool newline) {
    if (token > TokenType::INFIX_BEGIN && token < TokenType::INFIX_END)
        return &Parser::ParseInfix;

    if (!newline) {
        // They must necessarily appear on the same line!
        switch (token) {
            case TokenType::LEFT_INIT:
                return &Parser::ParseInit;
            case TokenType::LEFT_SQUARE:
                return &Parser::ParseSubscript;
            default:
                break;
        }
    }

    switch (token) {
        case TokenType::COMMA:
            return &Parser::ParseExpressionList;
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
        case TokenType::SCOPE:
            return &Parser::ParseSelector;
        case TokenType::ELVIS:
            return &Parser::ParseElvis;
        case TokenType::EQUAL:
        case TokenType::ASSIGN_ADD:
        case TokenType::ASSIGN_SUB:
            return &Parser::ParseAssignment;
        case TokenType::KW_IN:
        case TokenType::KW_NOT:
            return &Parser::ParseIn;
        case TokenType::MINUS_MINUS:
        case TokenType::PLUS_PLUS:
            return &Parser::ParsePostInc;
        case TokenType::NULL_COALESCING:
            return &Parser::ParseNullCoalescing;
        case TokenType::PIPELINE:
            return &Parser::ParsePipeline;
        case TokenType::QUESTION:
            return &Parser::ParseTernary;
        case TokenType::WALRUS:
            return &Parser::ParseWalrus;
        default:
            return nullptr;
    }

    assert(false);
}

Parser::NudMeth Parser::LookupNUD(TokenType token) {
    if (token > TokenType::LITERAL_BEGIN && token < TokenType::LITERAL_END)
        return &Parser::ParseLiteral;

    switch (token) {
        case TokenType::ARROW_LEFT:
            return &Parser::ParseChanOut;
        case TokenType::EXCLAMATION:
        case TokenType::MINUS:
        case TokenType::PLUS:
        case TokenType::TILDE:
            return &Parser::ParsePrefix;
        case TokenType::LEFT_BRACES:
            return &Parser::ParseDictSet;
        case TokenType::LEFT_ROUND:
            return &Parser::ParseArrowOrTuple;
        case TokenType::LEFT_SQUARE:
            return &Parser::ParseList;
        case TokenType::KW_AWAIT:
            return &Parser::ParseAwait;
        case TokenType::KW_TRAP:
            return &Parser::ParseTrap;
        default:
            return nullptr;
    }

    assert(false);
}

Node *Parser::ParseArrowOrTuple(Context *context) {
    ARC a_items;
    ARC body;

    Position start = TKCUR_START;

    this->Eat(true);

    auto *items = this->ParseFnParams(context, true);

    a_items = items;

    this->EatNL();

    auto end = TKCUR_END;

    if (!this->MatchEat(TokenType::RIGHT_ROUND, false))
        throw ParserException(TKCUR_LOC, kStandardError[24]);

    this->IgnoreNewLineIF(TokenType::FAT_ARROW);

    if (!this->Match(TokenType::FAT_ARROW)) {
        for (int i = 0; i < items->length; i++) {
            if (AR_TYPEOF(items->objects[i], type_ast_parameter_))
                throw ParserException(((Node *) items->objects[i])->loc, kStandardError[0]);
        }

        if (items->length == 1)
            return (Node *) ListGet(items, 0);

        auto *tuple = NewNode<Unary>(type_ast_unary_, false, NodeType::TUPLE);
        if (tuple == nullptr)
            throw DatatypeException();

        tuple->loc.start = start;
        tuple->loc.end = end;

        tuple->value = a_items.Unwrap();

        return (Node *) tuple;
    }

    this->Eat(true);

    // Check param list
    for (int i = 0; i < items->length; i++) {
        if (AR_TYPEOF(items->objects[i], type_ast_identifier_)) {
            // Convert Identifier to parameter
            auto *identifier = (Unary *) items->objects[i];

            auto *param = NewNode<Parameter>(type_ast_parameter_, false, NodeType::PARAMETER);
            if (param == nullptr)
                throw DatatypeException();

            param->loc = identifier->loc;
            param->id = (String *) IncRef(identifier->value);

            Release(items->objects[i]);
            items->objects[i] = (ArObject *) param;

            continue;
        }

        if (!AR_TYPEOF(items->objects[i], type_ast_parameter_))
            throw ParserException(((Node *) items->objects[i])->loc, kStandardError[0]);
    }

    Context f_ctx(context, ContextType::FUNC);

    body = this->ParseBlock(&f_ctx);

    auto *func = NewNode<Function>(type_ast_function_, false, NodeType::FUNCTION);
    if (func == nullptr)
        throw DatatypeException();

    func->doc = IncRef(f_ctx.doc);

    func->params = (List *) a_items.Unwrap();
    func->body = (Node *) body.Unwrap();

    func->loc.start = start;
    func->loc.end = func->body->loc.end;

    return (Node *) func;
}

Node *Parser::ParseAssignment(Context *context, Node *left) {
    auto type = TKCUR_TYPE;

    this->Eat(true);

    if (left->node_type != NodeType::IDENTIFIER
        && left->node_type != NodeType::INDEX
        && left->node_type != NodeType::SLICE
        && left->node_type != NodeType::TUPLE
        && left->node_type != NodeType::SELECTOR)
        throw ParserException(left->loc, kStandardError[28]);

    // Check for tuple content
    if (left->node_type == NodeType::TUPLE) {
        const auto *tuple = (List *) ((Unary *) left)->value;

        for (ArSize i = 0; i < tuple->length; i++) {
            const auto *itm = (Node *) tuple->objects[i];
            if (itm->node_type != NodeType::IDENTIFIER
                && itm->node_type != NodeType::INDEX
                && left->node_type != NodeType::SLICE
                && itm->node_type != NodeType::SELECTOR)
                throw ParserException(itm->loc, kStandardError[28]);
        }
    }

    auto *expr = this->ParseExpression(context, PeekPrecedence(TokenType::EQUAL));

    auto *assignment = NewNode<Assignment>(type_ast_vardecl_, false, NodeType::ASSIGNMENT);
    if (assignment == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    assignment->loc.start = left->loc.start;
    assignment->loc.end = expr->loc.end;

    assignment->name = (ArObject *) IncRef(left);
    assignment->value = (ArObject *) expr;

    return (Node *) assignment;
}

Node *Parser::ParseAwait(Context *context) {
    auto start = TKCUR_START;

    this->Eat(true);

    // TODO: Fix precedence
    auto *expr = this->ParseExpression(context, 0);

    auto *unary = NewNode<Unary>(type_ast_unary_, false, NodeType::AWAIT);
    if (unary == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    unary->loc.start = start;
    unary->loc.end = expr->loc.end;

    unary->value = (ArObject *) expr;

    return (Node *) unary;
}

Node *Parser::ParseChanOut(Context *context) {
    Position start = TKCUR_START;

    this->Eat(true);

    auto expr = this->ParseExpression(context, PeekPrecedence(TokenType::ARROW_LEFT));

    auto *unary = NewNode<Unary>(type_ast_unary_, false, NodeType::CHAN_OUT);
    if (unary == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    unary->loc.start = start;
    unary->loc.end = expr->loc.end;

    unary->value = (ArObject *) expr;

    return (Node *) unary;
}

Node *Parser::ParseDictSet(Context *context) {
    ARC a_list;

    Position start = TKCUR_START;

    // It does not matter, it is a placeholder.
    // The important thing is that it is not DICT or SET.
    NodeType type = NodeType::ASSIGNMENT;

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    a_list = list;

    this->Eat(true);

    if (this->Match(TokenType::RIGHT_BRACES)) {
        auto *ret = NewNode<Unary>(type_ast_unary_, false, NodeType::DICT);
        if (ret == nullptr)
            throw DatatypeException();

        ret->loc.start = start;
        ret->loc.end = TKCUR_END;

        this->Eat(false);

        return (Node *) ret;
    }

    do {
        auto *expr = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

        if (!ListAppend(list, (ArObject *) expr)) {
            Release(expr);

            throw DatatypeException();
        }

        Release(expr);

        if (this->MatchEat(TokenType::COLON, true)) {
            if (type == NodeType::SET)
                throw ParserException(TKCUR_LOC, kStandardError[21]);

            type = NodeType::DICT;

            this->EatNL();

            expr = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

            if (!ListAppend(list, (ArObject *) expr)) {
                Release(expr);

                throw DatatypeException();
            }

            Release(expr);

            continue;
        }

        if (type == NodeType::DICT)
            throw ParserException(TKCUR_LOC, kStandardError[22]);

        type = NodeType::SET;
    } while (this->MatchEat(TokenType::COMMA, true));

    auto end = TKCUR_END;

    if (!this->MatchEat(TokenType::RIGHT_BRACES, false))
        throw ParserException(TKCUR_LOC, kStandardError[23], type == NodeType::DICT ? "dict" : "set");

    auto *unary = NewNode<Unary>(type_ast_unary_, false, type);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;
    unary->loc.end = end;

    unary->value = a_list.Unwrap();

    return (Node *) unary;
}

Node *Parser::ParseElvis(Context *context, Node *left) {
    this->Eat(true);

    auto *expr = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

    auto *elvis = NewNode<Binary>(type_ast_binary_, false, NodeType::ELVIS);
    if (elvis == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    elvis->loc.start = left->loc.start;
    elvis->loc.end = expr->loc.end;

    elvis->left = (ArObject *) IncRef(left);
    elvis->right = (ArObject *) expr;

    return (Node *) elvis;
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

    auto *token = (const Token *) &this->tkcur_;
    auto nline = false;

    if (this->Match(TokenType::END_OF_LINE)) {
        if (!this->scanner_.PeekToken(&token))
            throw ScannerException();

        nline = true;
    }

    auto tk_type = token->type;
    while (precedence < Parser::PeekPrecedence(tk_type)) {
        if ((led = Parser::LookupLED(tk_type, nline)) == nullptr)
            break;

        if (nline)
            this->EatNL();

        nline = false;

        left = (this->*led)(context, ((Node *) left.Get()));

        tk_type = TKCUR_TYPE;

        if (this->Match(TokenType::END_OF_LINE)) {
            if (!this->scanner_.PeekToken(&token))
                throw ScannerException();

            nline = true;
            tk_type = token->type;
        }
    }

    // TODO: safe?!

    return (Node *) left.Unwrap();
}

Node *Parser::ParseExpressionList(Context *context, Node *left) {
    ARC a_list;

    Position end{};

    auto precedence = PeekPrecedence(TokenType::COMMA);

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    a_list = list;

    if (!ListAppend(list, (ArObject *) left))
        throw DatatypeException();

    this->Eat(false);

    do {
        this->EatNL();

        auto *expr = this->ParseExpression(context, precedence);

        if (!ListAppend(list, (ArObject *) expr)) {
            Release(expr);

            throw DatatypeException();
        }

        end = expr->loc.end;

        Release(expr);

        this->IgnoreNewLineIF(TokenType::COMMA);
    } while (this->MatchEat(TokenType::COMMA, false));

    auto *tuple = NewNode<Unary>(type_ast_unary_, false, NodeType::TUPLE);
    if (tuple == nullptr)
        throw DatatypeException();

    tuple->loc.start = left->loc.start;
    tuple->loc.end = end;

    tuple->value = a_list.Unwrap();

    return (Node *) tuple;
}

Node *Parser::ParseIdentifier([[maybe_unused]]Context *context) {
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

Node *Parser::ParseIn(Context *context, Node *left) {
    auto kind = NodeType::IN;

    if (TKCUR_TYPE == TokenType::KW_NOT) {
        kind = NodeType::NOT_IN;

        this->Eat(true);
    }

    this->Eat(true);

    auto *expr = this->ParseExpression(context, PeekPrecedence(TokenType::KW_IN));

    auto *in = NewNode<Binary>(type_ast_binary_, false, kind);
    if (in == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    in->loc.start = left->loc.start;
    in->loc.end = expr->loc.end;

    in->left = (ArObject *) IncRef(left);
    in->right = (ArObject *) expr;

    return (Node *) in;
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

    infix->token_type = type;

    infix->left = (ArObject *) IncRef(left);
    infix->right = (ArObject *) right;

    return (Node *) infix;
}

Node *Parser::ParseInit(Context *context, Node *left) {
    ARC a_list;

    this->Eat(true);

    if (this->Match(TokenType::RIGHT_ROUND)) {
        auto *init = NewNode<ObjectInit>(type_ast_objinit_, false, NodeType::OBJ_INIT);
        if (init == nullptr)
            throw DatatypeException();

        init->loc.start = left->loc.start;
        init->loc.end = TKCUR_END;

        init->left = IncRef(left);

        this->Eat(false);

        return (Node *) init;
    }

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    a_list = list;

    int count = 0;
    bool kwargs = false;

    do {
        auto *key = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

        if (!ListAppend(list, (ArObject *) key)) {
            Release(key);

            throw DatatypeException();
        }

        Release(key);

        this->EatNL();

        count++;
        if (this->MatchEat(TokenType::EQUAL, true)) {
            if (key->node_type != NodeType::IDENTIFIER)
                throw ParserException(key->loc, kStandardError[0]);

            if (--count != 0)
                throw ParserException(TKCUR_LOC, kStandardError[30]);

            auto *value = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

            if (!ListAppend(list, (ArObject *) value)) {
                Release(value);

                throw DatatypeException();
            }

            Release(value);

            this->EatNL();

            kwargs = true;

            continue;
        }

        if (kwargs)
            throw ParserException(TKCUR_LOC, kStandardError[30]);
    } while (this->MatchEat(TokenType::COMMA, true));

    if(!this->Match(TokenType::RIGHT_ROUND))
        throw ParserException(TKCUR_LOC, kStandardError[31]);

    auto *init = NewNode<ObjectInit>(type_ast_objinit_, false, NodeType::OBJ_INIT);
    if (init == nullptr)
        throw DatatypeException();

    init->loc.start = left->loc.start;
    init->loc.end = TKCUR_END;

    init->left = IncRef(left);
    init->values = a_list.Unwrap();

    init->as_map = kwargs;

    this->Eat(false);

    return (Node *) init;
}

Node *Parser::ParseList(Context *context) {
    ARC a_list;

    Position start = TKCUR_START;

    this->Eat(true);

    auto *list = ListNew();
    if (list == nullptr)
        throw DatatypeException();

    a_list = list;

    if (!this->Match(TokenType::RIGHT_SQUARE)) {
        do {
            this->EatNL();

            auto *itm = this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

            if (!ListAppend(list, (ArObject *) itm)) {
                Release(itm);

                throw DatatypeException();
            }

            Release(itm);
        } while (this->MatchEat(TokenType::COMMA, true));
    }

    auto end = TKCUR_END;

    if (!this->MatchEat(TokenType::RIGHT_SQUARE, false))
        throw ParserException(TKCUR_LOC, kStandardError[20], "list");

    auto *unary = NewNode<Unary>(type_ast_unary_, false, NodeType::LIST);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;
    unary->loc.end = end;

    unary->value = a_list.Unwrap();

    return (Node *) unary;
}

Node *Parser::ParseNullCoalescing(Context *context, Node *left) {
    this->Eat(true);

    auto *expr = this->ParseExpression(context, PeekPrecedence(TokenType::NULL_COALESCING));

    auto *n_coal = NewNode<Binary>(type_ast_binary_, false, NodeType::ELVIS);
    if (n_coal == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    n_coal->loc.start = left->loc.start;
    n_coal->loc.end = expr->loc.end;

    n_coal->left = (ArObject *) IncRef(left);
    n_coal->right = (ArObject *) expr;

    return (Node *) n_coal;
}

Node *Parser::ParsePipeline(Context *context, Node *left) {
    this->Eat(true);

    auto *right = this->ParseExpression(context, PeekPrecedence(TokenType::PIPELINE));
    if (right->node_type == NodeType::CALL) {
        auto *call = (Call *) right;

        if (!ListPrepend((List *) call->args, (ArObject *) left)) {
            Release(right);

            throw DatatypeException();
        }

        right->loc.start = left->loc.start;

        return (Node *) right;
    }

    auto *args = ListNew();
    if (args == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    if (!ListAppend(args, (ArObject *) left)) {
        Release(args);
        Release(right);

        throw DatatypeException();
    }

    auto *call = NewNode<Call>(type_ast_call_, false, NodeType::CALL);
    if (call == nullptr) {
        Release(args);
        Release(right);

        throw DatatypeException();
    }

    call->loc.start = left->loc.start;
    call->loc.end = right->loc.end;

    call->left = IncRef(right);
    call->args = (ArObject *) args;

    return (Node *) call;
}

Node *Parser::ParsePostInc([[maybe_unused]]Context *context, Node *left) {
    if (left->node_type != NodeType::IDENTIFIER
        && left->node_type != NodeType::INDEX
        && left->node_type != NodeType::SELECTOR)
        throw ParserException(TKCUR_LOC, kStandardError[26]);

    auto *unary = NewNode<Unary>(type_ast_update_, false, NodeType::UPDATE);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = left->loc.start;
    unary->loc.end = TKCUR_END;

    unary->value = (ArObject *) IncRef(left);

    return (Node *) unary;
}

Node *Parser::ParsePrefix(Context *context) {
    Position start = TKCUR_START;
    TokenType kind = TKCUR_TYPE;

    this->Eat(true);

    auto *right = this->ParseExpression(context, PeekPrecedence(kind));

    auto *unary = NewNode<Unary>(type_ast_prefix_, false, NodeType::PREFIX);
    if (unary == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    unary->loc.start = start;
    unary->loc.end = unary->loc.end;

    unary->token_type = kind;

    unary->value = (ArObject *) right;

    return (Node *) unary;
}

Node *Parser::ParseSelector([[maybe_unused]]Context *context, Node *left) {
    auto kind = TKCUR_TYPE;

    this->Eat(true);

    if (!this->CheckIDExt())
        throw ParserException(TKCUR_LOC, kStandardError[27],
                              kind == TokenType::DOT
                              ? "."
                              : (kind == TokenType::QUESTION_DOT
                                 ? "?."
                                 : "::"
                              )
        );

    auto *right = Parser::ParseIdentifierSimple(&this->tkcur_);

    auto *selector = NewNode<Binary>(type_ast_selector_, false, NodeType::SELECTOR);
    if (selector == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    selector->loc.start = left->loc.start;
    selector->loc.end = TKCUR_END;

    selector->token_type = kind;

    selector->left = (ArObject *) IncRef(left);
    selector->right = (ArObject *) right;

    this->Eat(false);

    return (Node *) selector;
}

Node *Parser::ParseSubscript(Context *context, Node *left) {
    ARC start;
    ARC stop;

    this->Eat(true);

    if (this->Match(TokenType::RIGHT_SQUARE))
        throw ParserException(TKCUR_LOC, kStandardError[25]);

    if (!this->Match(TokenType::COLON))
        start = this->ParseExpression(context, 0);

    if (this->MatchEat(TokenType::COLON, true) && !this->Match(TokenType::RIGHT_SQUARE))
        stop = (ArObject *) this->ParseExpression(context, 0);

    this->EatNL();

    if (!this->Match(TokenType::RIGHT_SQUARE))
        throw ParserException(TKCUR_LOC, kStandardError[20], stop ? "slice" : "index");

    auto *slice = NewNode<Subscript>(type_ast_subscript_, false, stop ? NodeType::SLICE : NodeType::INDEX);
    if (slice == nullptr)
        throw DatatypeException();

    slice->loc.start = left->loc.start;
    slice->loc.end = TKCUR_END;

    slice->expression = IncRef(left);
    slice->start = (Node *) start.Unwrap();
    slice->stop = (Node *) stop.Unwrap();

    this->Eat(false);

    return (Node *) slice;
}

Node *Parser::ParseTernary(Context *context, Node *left) {
    ARC body;
    ARC orelse;

    this->Eat(true);

    body = (ArObject *) this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));

    this->IgnoreNewLineIF(TokenType::COLON);

    if (this->MatchEat(TokenType::COLON, false)) {
        this->EatNL();

        orelse = (ArObject *) this->ParseExpression(context, PeekPrecedence(TokenType::COMMA));
    }

    auto *branch = NewNode<Branch>(type_ast_branch_, false, NodeType::TERNARY);
    if (branch == nullptr)
        throw DatatypeException();

    branch->test = IncRef(left);
    branch->body = (Node *) body.Unwrap();
    branch->orelse = (Node *) orelse.Unwrap();

    branch->loc.start = left->loc.start;
    branch->loc.end = branch->orelse->loc.end;

    return (Node *) branch;
}

Node *Parser::ParseTrap(Context *context) {
    Position start = TKCUR_START;

    this->Eat(true);

    auto *right = this->ParseExpression(context);

    // Expressions with multiple traps are useless,
    // if the expression is already a trap, return it immediately
    if (right->node_type == NodeType::TRAP)
        return right;

    auto *unary = NewNode<Unary>(type_ast_unary_, false, NodeType::TRAP);
    if (unary == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    unary->loc.start = start;
    unary->loc.end = unary->loc.end;

    unary->value = (ArObject *) right;

    return (Node *) unary;
}

Node *Parser::ParseWalrus(Context *context, node::Node *left) {
    ARC tmp;

    this->Eat(true);

    // Check left
    if (left->node_type == NodeType::IDENTIFIER) {
        tmp = IncRef(((Unary *) left)->value);
    } else if (left->node_type == NodeType::TUPLE) {
        const auto *tuple = (List *) ((Unary *) left)->value;

        auto list = ListNew(tuple->length);
        if (list == nullptr)
            throw DatatypeException();

        for (ArSize i = 0; i < tuple->length; i++) {
            auto *itm = (Node *) tuple->objects[i];

            if (itm->node_type != NodeType::IDENTIFIER) {
                Release(list);

                throw ParserException(itm->loc, kStandardError[29], ":=");
            }

            ListAppend(list, (ArObject *) itm);
        }

        tmp = (ArObject *) list;
    } else
        throw ParserException(left->loc, kStandardError[0]);

    auto *right = this->ParseExpression(context, PeekPrecedence(TokenType::WALRUS));

    auto *vardecl = NewNode<Assignment>(type_ast_vardecl_, false, NodeType::VARDECL);
    if (vardecl == nullptr) {
        Release(right);

        throw DatatypeException();
    }

    vardecl->loc.start = left->loc.start;
    vardecl->loc.end = right->loc.end;

    vardecl->name = (ArObject *) tmp.Unwrap();
    vardecl->value = (ArObject *) right;

    vardecl->multi = true;

    return (Node *) vardecl;
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