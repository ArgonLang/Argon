// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <cctype>

#include <argon/vm/datatype/atom.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/decimal.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/list.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/stringbuilder.h>

#include <argon/lang/parser/parsererr.h>
#include <argon/lang/parser/parser.h>

using namespace argon::lang::parser;
using namespace argon::lang::scanner;
using namespace argon::vm::datatype;

#define TKCUR_TYPE (this->tkcur_.type)

Node *MakeIdentifier(const Token *token) {
    auto *str = StringNew((const char *) token->buffer, token->length);
    if (str == nullptr)
        return nullptr;

    auto *id = UnaryNew((ArObject *) str, NodeType::IDENTIFIER, token->loc);

    Release(str);

    return (Node *) id;
}

Node *MakeSafeExpr(Node *left) {
// Encapsulates "null safety" expressions, e.g.: a?.b, a.b?.c(), a?.b = c?.o
    auto *safe = UnaryNew((ArObject *) left, NodeType::SAFE_EXPR, left->loc);
    if (safe == nullptr)
        throw DatatypeException();

    safe->loc = ((Node *) safe->value)->loc;

    return (Node *) safe;
}

int Parser::PeekPrecedence(scanner::TokenType token) {
    switch (token) {
        case TokenType::WALRUS:
            return 10;
        case TokenType::EQUAL:
        case TokenType::ASSIGN_ADD:
        case TokenType::ASSIGN_SUB:
            return 20;
        case TokenType::COMMA:
            return 30;
        case TokenType::ARROW_RIGHT:
            return 40;
        case TokenType::ELVIS:
        case TokenType::QUESTION:
        case TokenType::NULL_COALESCING:
            return 50;
        case TokenType::PIPELINE:
            return 60;
        case TokenType::OR:
            return 70;
        case TokenType::AND:
            return 80;
        case TokenType::PIPE:
            return 90;
        case TokenType::CARET:
            return 100;
        case TokenType::KW_IN:
        case TokenType::KW_NOT:
            return 110;
        case TokenType::EQUAL_EQUAL:
        case TokenType::EQUAL_STRICT:
        case TokenType::NOT_EQUAL:
        case TokenType::NOT_EQUAL_STRICT:
            return 120;
        case TokenType::LESS:
        case TokenType::LESS_EQ:
        case TokenType::GREATER:
        case TokenType::GREATER_EQ:
            return 130;
        case TokenType::SHL:
        case TokenType::SHR:
            return 140;
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
            return 150;
        case TokenType::ASTERISK:
        case TokenType::SLASH:
        case TokenType::SLASH_SLASH:
        case TokenType::PERCENT:
            return 160;
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
        case TokenType::SCOPE:
            return 170;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
        case TokenType::LEFT_INIT:
        case TokenType::LEFT_BRACES:
        case TokenType::LEFT_SQUARE:
        case TokenType::LEFT_ROUND:
            return 180;
        default:
            return 1000;
    }
}

Parser::LedMeth Parser::LookupLed(lang::scanner::TokenType token) const {
    if (this->TokenInRange(TokenType::INFIX_BEGIN, TokenType::INFIX_END))
        return &Parser::ParseInfix;

    // They must necessarily appear on the same line!
    switch (this->tkcur_.type) {
        case TokenType::LEFT_ROUND:
            return &Parser::ParseFnCall;
        case TokenType::LEFT_SQUARE:
            return &Parser::ParseSubscript;
        case TokenType::LEFT_INIT:
            return &Parser::ParseInit;
        default:
            break;
    }

    // They can appear in a new line while remaining valid!
    // (See ParseExpression code to better understand)
    switch (token) {
        case TokenType::KW_IN:
        case TokenType::KW_NOT:
            return &Parser::ParseIn;
        case TokenType::COMMA:
            return &Parser::ParseExpressionList;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            return &Parser::ParsePostInc;
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
        case TokenType::SCOPE:
            return &Parser::ParseSelector;
        case TokenType::PIPELINE:
            return &Parser::ParsePipeline;
        case TokenType::ELVIS:
            return &Parser::ParseElvis;
        case TokenType::QUESTION:
            return &Parser::ParseTernary;
        case TokenType::NULL_COALESCING:
            return &Parser::ParseNullCoalescing;
        case TokenType::EQUAL:
        case TokenType::ASSIGN_ADD:
        case TokenType::ASSIGN_SUB:
            return &Parser::ParseAssignment;
        case TokenType::WALRUS:
            return &Parser::ParseWarlus;
        default:
            return nullptr;
    }
}

Parser::NudMeth Parser::LookupNud(lang::scanner::TokenType token) const {
    if (this->TokenInRange(TokenType::LITERAL_BEGIN, TokenType::LITERAL_END))
        return &Parser::ParseLiteral;

    switch (token) {
        case scanner::TokenType::KW_ASYNC:
            return &Parser::ParseAsync;
        case scanner::TokenType::KW_AWAIT:
            return &Parser::ParseAwait;
        case TokenType::IDENTIFIER:
        case TokenType::BLANK:
        case TokenType::SELF:
            return &Parser::ParseIdentifier;
        case TokenType::ARROW_LEFT:
            return &Parser::ParseChanGet;
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
            return &Parser::ParsePrefix;
        case TokenType::KW_TRAP:
            return &Parser::ParseTrap;
        case TokenType::LEFT_ROUND:
            return &Parser::ParseArrowOrTuple;
        case TokenType::LEFT_SQUARE:
            return &Parser::ParseList;
        case TokenType::LEFT_BRACES:
            return &Parser::ParseDictSet;
        default:
            return nullptr;
    }
}

ArObject *Parser::ParseParamList(bool parse_expr, bool *out_grouped_expr) {
    ARC params;
    ARC tmp;

    Position start{};

    const Argument *param;

    int count = 0;
    int mode = 0;

    params = (ArObject *) ListNew();
    if (!params)
        throw DatatypeException();

    do {
        this->IgnoreNL();

        if (this->Match(TokenType::RIGHT_ROUND))
            break;

        if (this->Match(TokenType::ELLIPSIS)) {
            if (mode > 1)
                throw ParserException("unexpected rest parameter");

            mode = 2;

            start = this->tkcur_.loc.start;

            this->Eat();

            if (!this->Match(TokenType::IDENTIFIER))
                throw ParserException("expected identifier after '...'");

            tmp = (ArObject *) this->ParseIDValue(NodeType::REST, start);
        } else if (this->Match(TokenType::AMPERSAND)) {
            if (mode > 2)
                throw ParserException("only one &-var is allowed per function declaration");

            mode = 3;

            start = this->tkcur_.loc.start;

            this->Eat();

            if (!this->Match(TokenType::IDENTIFIER))
                throw ParserException("expected identifier after &");

            tmp = (ArObject *) this->ParseIDValue(NodeType::KWARG, start);
        } else {
            if (mode > 1)
                throw ParserException("unexpected var/var-keyword parameter");

            tmp = (ArObject *) this->ParseIDNamedParam(parse_expr);

            param = (Argument *) tmp.Get();

            if (mode > 0 && param->value == nullptr)
                throw ParserException("unexpected non keyword parameter");

            if (param->node_type == NodeType::ARGUMENT && param->value != nullptr)
                mode = 1;
        }

        if (!ListAppend((List *) params.Get(), tmp.Get()))
            throw DatatypeException();

        this->IgnoreNL();

        if (this->Match(TokenType::COMMA))
            count++;

    } while (this->MatchEat(TokenType::COMMA));

    if (out_grouped_expr != nullptr)
        *out_grouped_expr = count == 0;

    return params.Unwrap();
}

ArObject *Parser::ParseTraitList() {
    ARC list;

    list = (ArObject *) ListNew();
    if (!list)
        throw DatatypeException();

    do {
        this->IgnoreNL();

        auto *scope = this->ParseScope();

        if (!ListAppend((List *) list.Get(), (ArObject *) scope)) {
            Release(scope);
            throw DatatypeException();
        }

        Release(scope);

        this->IgnoreNL();
    } while (this->MatchEat(TokenType::COMMA));

    return list.Unwrap();
}

bool Parser::ParseFnCallRestArgs(Node *expr, ARC &args, bool must_parse) {
    Loc loc{};

    loc.start = expr->loc.start;
    loc.end = this->tkcur_.loc.end;

    if (!this->MatchEat(TokenType::ELLIPSIS)) {
        if (must_parse)
            throw ParserException("parameters to a function must be passed in the order: "
                                  "[positional][, named param][, spread][, kwargs]");
        return false;
    }

    auto *rest = UnaryNew((ArObject *) expr, NodeType::ELLIPSIS, loc);
    if (rest == nullptr)
        throw DatatypeException();

    if (!ListAppend((List *) args.Get(), (ArObject *) rest)) {
        Release(rest);

        throw DatatypeException();
    }

    Release(rest);

    return true;
}

bool Parser::ParseFnCallNamedArg(Node *expr, ARC &kwargs, bool must_parse) {
    if (!this->MatchEat(TokenType::EQUAL)) {
        if (must_parse)
            throw ParserException("parameters to a function must be passed in the order: "
                                  "[positional][, named param][, spread][, kwargs]");

        return false;
    }

    // Sanity check
    if (expr->node_type != NodeType::IDENTIFIER)
        throw ParserException("only identifiers are allowed before the '=' sign");

    this->IgnoreNL();

    if (!kwargs)
        kwargs = (ArObject *) ListNew();

    if (!kwargs)
        throw DatatypeException();

    auto *value = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

    auto *arg = (ArObject *) ArgumentNew((Unary *) expr, value, NodeType::ARGUMENT);

    Release(value);

    if (arg == nullptr)
        throw DatatypeException();

    if (!ListAppend((List *) kwargs.Get(), arg)) {
        Release(arg);

        throw DatatypeException();
    }

    Release(arg);

    return true;
}

bool Parser::ParseFnCallUnpack(ARC &kwargs, bool must_parse) {
    Position start = this->tkcur_.loc.start;
    ARC expr;

    if (!this->MatchEat(TokenType::AMPERSAND)) {
        if (must_parse)
            throw ParserException("parameters to a function must be passed in the order: "
                                  "[positional][, named param][, spread][, kwargs]");
        return false;
    }

    if (!kwargs)
        kwargs = (ArObject *) ListNew();

    if (!kwargs)
        throw DatatypeException();

    expr = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::COMMA));

    auto *arg = ArgumentNew(nullptr, (Node *) expr.Get(), NodeType::ARGUMENT);
    if (arg == nullptr)
        throw DatatypeException();

    arg->loc.start = start;

    if (!ListAppend((List *) kwargs.Get(), (ArObject *) arg)) {
        Release(arg);

        throw DatatypeException();
    }

    Release(arg);

    return true;
}

bool Parser::ScopeExactMatch(ParserScope scope) const {
    return this->scope_stack_ != nullptr && this->scope_stack_->scope == scope;
}

bool Parser::ScopeMatch(ParserScope scope) const {
    auto *cursor = this->scope_stack_;

    while (cursor != nullptr) {
        if (cursor->scope == scope)
            return true;

        cursor = cursor->prev;
    }

    return false;
}

Node *Parser::ParseAssertion() {
    ARC expr;
    ARC msg;

    Position start = this->tkcur_.loc.start;

    this->Eat();
    this->IgnoreNL();

    expr = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::COMMA));

    this->IgnoreNewLineIF(TokenType::COMMA);

    if (this->MatchEat(TokenType::COMMA)) {
        this->IgnoreNL();
        msg = (ArObject *) this->ParseExpression(0);
    }

    auto *asrt = BinaryNew((Node *) expr.Get(), (Node *) msg.Get(), TokenType::TK_NULL, NodeType::ASSERT);
    if (asrt == nullptr)
        throw DatatypeException();

    asrt->loc = this->tkcur_.loc;

    asrt->loc.start = start;

    return (Node *) asrt;
}

Node *Parser::ParseAssignment(Node *left) {
    TokenType type = TKCUR_TYPE;

    this->Eat();
    this->IgnoreNL();

    if (left->node_type != NodeType::IDENTIFIER &&
        left->node_type != NodeType::INDEX &&
        left->node_type != NodeType::SLICE &&
        left->node_type != NodeType::TUPLE &&
        left->node_type != NodeType::SELECTOR)
        throw ParserException("expected identifier or list to the left of the assignment expression");

    // Check for tuple content
    if (left->node_type == NodeType::TUPLE) {
        const auto *tuple = (List *) ((Unary *) left)->value;

        for (ArSize i = 0; i < tuple->length; i++) {
            const auto *itm = (Node *) tuple->objects[i];
            if (itm->node_type != NodeType::IDENTIFIER &&
                itm->node_type != NodeType::INDEX &&
                itm->node_type != NodeType::SELECTOR)
                throw ParserException(
                        "expected identifier, subscript or selector to the left of the assignment expression");
        }
    }

    auto *expr = this->ParseExpression(PeekPrecedence(TokenType::EQUAL));

    auto *assign = BinaryNew(left, expr, type, NodeType::ASSIGNMENT);
    if (assign == nullptr) {
        Release(expr);
        throw DatatypeException();
    }

    Release(expr);

    return (Node *) assign;
}

Node *Parser::ParseAsync(bool pub) {
    Position start = this->tkcur_.loc.start;

    this->Eat();
    this->IgnoreNL();

    auto *func = (Function *) this->ParseFn(pub);

    func->async = true;
    func->loc.start = start;

    return (Node *) func;
}

Node *Parser::ParseAsync() {
    Position start = this->tkcur_.loc.start;
    Node *expr;

    this->Eat();
    this->IgnoreNL();

    expr = this->ParseExpression(PeekPrecedence(TokenType::LEFT_ROUND));
    if (expr->node_type != NodeType::FUNC)
        throw ParserException("expected function after async keyword");

    auto *func = (Function *) expr;
    func->async = true;
    func->loc.start = start;

    return expr;
}

Node *Parser::ParseArrowOrTuple() {
    ARC items;

    Position start = this->tkcur_.loc.start;
    Position end{};

    bool grouped_expression;

    this->Eat();
    this->IgnoreNL();

    items = this->ParseParamList(true, &grouped_expression);

    this->IgnoreNL();

    end = this->tkcur_.loc.end;

    if (!this->MatchEat(scanner::TokenType::RIGHT_ROUND))
        throw ParserException("expected ')' after tuple/function definition");

    this->IgnoreNewLineIF(TokenType::FAT_ARROW);

    if (this->MatchEat(scanner::TokenType::FAT_ARROW)) {
        // Check param list
        const auto *list = (List *) items.Get();
        for (ArSize i = 0; i < list->length; i++) {
            const auto *node = (Node *) list->objects[i];
            if (node->node_type != NodeType::ARGUMENT &&
                node->node_type != NodeType::REST &&
                node->node_type != NodeType::KWARG)
                throw ParserException("expression not allowed here");
        }

        this->EnterDocContext();

        auto *body = this->ParseBlock(ParserScope::BLOCK);

        auto *func = FunctionNew(nullptr, (List *) items.Get(), body, false);
        if (func == nullptr) {
            Release(body);
            throw DatatypeException();
        }

        func->loc.start = start;
        func->doc = this->doc_string_->Unwrap();

        Release(body);

        this->ExitDocContext();

        return (Node *) func;
    }

    auto *list = (List *) items.Get();
    for (ArSize i = 0; i < list->length; i++) {
        auto *node = (Node *) list->objects[i];

        if (node->node_type == NodeType::ARGUMENT) {
            if (((Argument *) node)->value != nullptr)
                throw ParserException("unexpected keyword parameter in tuple expression");

            list->objects[i] = (ArObject *) IncRef(((Argument *) node)->id);

            Release(node);

            continue;
        }

        if (node->node_type == NodeType::REST)
            throw ParserException("unexpected rest operator");
        else if (node->node_type == NodeType::KWARG)
            throw ParserException("unexpected kwarg operator");
    }

    if (grouped_expression && list->length > 0)
        return (Node *) ListGet(list, 0);

    auto *unary = UnaryNew(items.Get(), NodeType::TUPLE, this->tkcur_.loc);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;
    unary->loc.end = end;

    return (Node *) unary;
}

Node *Parser::ParseAwait() {
    Loc loc = this->tkcur_.loc;
    Node *expr = nullptr;

    this->Eat();
    this->IgnoreNL();

    expr = this->ParseExpression(40);

    loc.end = expr->loc.end;

    auto *unary = UnaryNew((ArObject *) expr, NodeType::AWAIT, loc);

    Release(expr);

    if (unary == nullptr)
        throw DatatypeException();

    return (Node *) unary;
}

Node *Parser::ParseBCFLabel() {
    Loc loc = this->tkcur_.loc;
    TokenType type = TKCUR_TYPE;
    Node *id = nullptr;

    this->Eat();

    if (this->Match(TokenType::IDENTIFIER)) {
        if (type == scanner::TokenType::KW_FALLTHROUGH)
            throw ParserException("unexpected label after fallthrough");

        id = this->ParseIdentifier();
    }

    auto *unary = UnaryNew((ArObject *) id, NodeType::JUMP, this->tkcur_.loc);
    if (unary == nullptr) {
        Release(id);
        throw DatatypeException();
    }

    unary->loc = loc;
    unary->token_type = type;

    if (id != nullptr)
        unary->loc.end = id->loc.end;

    Release(id);

    return (Node *) unary;
}

Node *Parser::ParseBlock(ParserScope scope) {
    ARC stmts;

    Position start = this->tkcur_.loc.start;

    if (!this->MatchEat(TokenType::LEFT_BRACES))
        throw ParserException("expected '{");

    stmts = (ArObject *) ListNew();
    if (!stmts)
        throw DatatypeException();

    this->IgnoreNL();
    while (!this->Match(TokenType::RIGHT_BRACES)) {
        this->IgnoreNL();

        auto *stmt = this->ParseDecls(scope);

        if (!ListAppend((List *) stmts.Get(), (ArObject *) stmt)) {
            Release(stmt);
            throw DatatypeException();
        }

        Release(stmt);

        this->IgnoreNL();
    }

    auto *block = UnaryNew(stmts.Get(), NodeType::BLOCK, this->tkcur_.loc);
    if (block == nullptr)
        throw DatatypeException();

    this->Eat();

    block->loc.start = start;

    return (Node *) block;
}

Node *Parser::ParseChanGet() {
    Position start = this->tkcur_.loc.start;

    this->Eat();
    this->IgnoreNL();

    auto expr = this->ParseExpression(PeekPrecedence(TokenType::ASTERISK));

    auto unary = UnaryNew((ArObject *) expr, TokenType::ARROW_LEFT, expr->loc);
    if (unary == nullptr) {
        Release(expr);

        throw DatatypeException();
    }

    unary->loc.start = start;

    return (Node *) unary;
}

Node *Parser::ParseDecls(ParserScope scope) {
    ARC stmt;
    Position start = this->tkcur_.loc.start;
    bool pub = false;

    ScopeEntry entry(scope);

    this->ScopePush(&entry);

    if (this->MatchEat(TokenType::KW_PUB)) {
        pub = true;

        if (scope != ParserScope::MODULE &&
            scope != ParserScope::STRUCT &&
            scope != ParserScope::TRAIT &&
            (scope != ParserScope::IF || entry.prev->scope != ParserScope::MODULE))
            throw ParserException("unexpected use of 'pub' modifier in this context");

        this->IgnoreNL();
    }

    switch (TKCUR_TYPE) {
        case TokenType::KW_IMPORT:
            if (scope != ParserScope::MODULE)
                throw ParserException("import not supported in this context");

            stmt = (ArObject *) this->ParseImport(pub);
            break;
        case TokenType::KW_FROM:
            if (scope != ParserScope::MODULE)
                throw ParserException("from-import not supported in this context");

            stmt = (ArObject *) this->ParseFromImport(pub);
            break;
        case TokenType::KW_WEAK:
            if (scope != ParserScope::STRUCT)
                throw ParserException("unexpected use of 'weak' in this context");

            this->Eat();
            this->IgnoreNL();

            if (!this->Match(scanner::TokenType::KW_VAR))
                throw ParserException("expected 'var' after weak keyword");

            stmt = (ArObject *) this->ParseVarDecl(pub, false, true);
            break;
        case TokenType::KW_VAR:
            if (scope == ParserScope::TRAIT)
                throw ParserException("unexpected use of 'var' in this context");

            stmt = (ArObject *) this->ParseVarDecl(pub, false, false);
            break;
        case TokenType::KW_LET:
            stmt = (ArObject *) this->ParseVarDecl(pub, true, false);
            break;
        case TokenType::KW_ASYNC:
            stmt = (ArObject *) this->ParseAsync(pub);
            break;
        case TokenType::KW_FUNC:
            stmt = (ArObject *) this->ParseFn(pub);
            break;
        case TokenType::KW_STRUCT:
            if (scope != ParserScope::BLOCK && scope != ParserScope::IF && scope != ParserScope::MODULE)
                throw ParserException("unexpected struct declaration");

            stmt = (ArObject *) this->ParseStructDecl(pub);
            break;
        case TokenType::KW_SYNC:
            if (scope == ParserScope::STRUCT || scope == ParserScope::TRAIT)
                throw ParserException("sync not supported in this context");

            stmt = (ArObject *) this->ParseSyncBlock();
            break;
        case TokenType::KW_TRAIT:
            if (scope != ParserScope::MODULE)
                throw ParserException("unexpected trait declaration");

            stmt = (ArObject *) this->ParseTraitDecl(pub);
            break;
        default:
            if (pub)
                throw ParserException("expected declaration after 'pub' keyword");

            if (scope == ParserScope::STRUCT || scope == ParserScope::TRAIT) {
                if (TKCUR_TYPE != TokenType::KW_IF)
                    throw ParserException("unexpected statement here");
            }

            stmt = (ArObject *) this->ParseStatement();
    }

    auto *s = (Node *) stmt.Unwrap();

    if (s != nullptr && pub)
        s->loc.start = start;

    this->ScopePop();

    return s;
}

Node *Parser::ParseDictSet() {
    Position start = this->tkcur_.loc.start;

    // It does not matter, it is a placeholder.
    // The important thing is that it is not DICT or SET.
    NodeType type = NodeType::BINARY;

    ARC list;

    this->Eat();
    this->IgnoreNL();

    list = (ArObject *) ListNew();
    if (!list)
        throw DatatypeException();

    if (this->Match(TokenType::RIGHT_BRACES)) {
        auto *ret = UnaryNew(list.Get(), NodeType::DICT, this->tkcur_.loc);
        if (ret == nullptr)
            throw DatatypeException();

        ret->loc.start = start;

        this->Eat();

        return (Node *) ret;
    }

    do {
        this->IgnoreNL();

        auto *expr = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

        if (!ListAppend((List *) list.Get(), (ArObject *) expr)) {
            Release(expr);
            throw DatatypeException();
        }

        Release(expr);

        this->IgnoreNL();

        if (this->MatchEat(TokenType::COLON)) {
            if (type == NodeType::SET)
                throw ParserException("you started defining a set, not a dict");

            type = NodeType::DICT;

            this->IgnoreNL();

            expr = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

            if (!ListAppend((List *) list.Get(), (ArObject *) expr)) {
                Release(expr);
                throw DatatypeException();
            }

            Release(expr);

            this->IgnoreNL();

            continue;
        }

        if (type == NodeType::DICT)
            throw ParserException("you started defining a dict, not a set");

        type = NodeType::SET;
    } while (this->MatchEat(TokenType::COMMA));

    auto *unary = UnaryNew(list.Get(), type, this->tkcur_.loc);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;

    if (!this->MatchEat(TokenType::RIGHT_BRACES)) {
        Release(unary);
        throw ParserException(type == NodeType::DICT ?
                              "expected '}' after dict definition" :
                              "expected '}' after set definition");
    }

    return (Node *) unary;
}

Node *Parser::ParseElvis(Node *left) {
    this->Eat();
    this->IgnoreNL();

    auto *expr = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

    auto *binary = BinaryNew(left, expr, TokenType::TK_NULL, NodeType::ELVIS);
    if (binary == nullptr) {
        Release(expr);
        throw DatatypeException();
    }

    return (Node *) binary;
}

Node *Parser::ParseExpression() {
    ARC expr;

    expr = (ArObject *) this->ParseExpression(0);

    if (this->Match(TokenType::COLON)) {
        if (((Node *) expr.Get())->node_type != NodeType::IDENTIFIER)
            throw ParserException("unexpected syntax");

        return (Node *) expr.Unwrap();
    }

    auto *ret = (Node *) expr.Unwrap();
    auto *inner = ret;

    // This trick allows us to check if there is an assignment expression under the Null Safety expression.
    if (inner->node_type == NodeType::SAFE_EXPR)
        inner = (Node *) ((Unary *) inner)->value;

    if (inner->node_type != NodeType::ASSIGNMENT && inner->node_type != NodeType::DECLARATION) {
        auto *unary = UnaryNew((ArObject *) ret, NodeType::EXPRESSION, this->tkcur_.loc);

        Release(ret);

        if (unary == nullptr)
            throw DatatypeException();

        unary->loc = ((Node *) unary->value)->loc;

        return (Node *) unary;
    }

    return ret;
}

Node *Parser::ParseExpression(int precedence) {
    bool is_safe = false;
    bool nl = false;

    const Token *token;

    LedMeth led;
    NudMeth nud;

    ARC left;

    if ((nud = this->LookupNud(TKCUR_TYPE)) == nullptr)
        left = (ArObject *) this->Unknown2Identifier();
    else
        left = (ArObject *) (this->*nud)();

    token = &this->tkcur_;

    if (this->Match(scanner::TokenType::END_OF_LINE)) {
        if (!this->scanner_.PeekToken(&token))
            throw ScannerException();

        nl = true;
    }

    while (precedence < Parser::PeekPrecedence(token->type)) {
        if ((led = this->LookupLed(token->type)) == nullptr)
            break;

        if (nl) {
            this->IgnoreNL();
            nl = false;
        }

        if (token->type == TokenType::QUESTION_DOT)
            is_safe = true;

        left = (ArObject *) (this->*led)((Node *) left.Get());

        token = &this->tkcur_;

        if (this->Match(scanner::TokenType::END_OF_LINE)) {
            if (!this->scanner_.PeekToken(&token))
                throw ScannerException();

            nl = true;
        }

        if (is_safe && Parser::PeekPrecedence(token->type) < PeekPrecedence(TokenType::DOT)) {
            left = (ArObject *) MakeSafeExpr((Node *) left.Get());
            is_safe = false;
        }
    }

    if (is_safe)
        return MakeSafeExpr((Node *) left.Get());

    return (Node *) left.Unwrap();
}

Node *Parser::ParseExpressionList(Node *left) {
    int precedence = PeekPrecedence(TokenType::COMMA);
    ARC list;
    Position end{};

    list = (ArObject *) ListNew();
    if (!list)
        throw DatatypeException();

    if (!ListAppend((List *) list.Get(), (ArObject *) left))
        throw DatatypeException();

    this->Eat();

    do {
        this->IgnoreNL();

        auto *expr = this->ParseExpression(precedence);

        if (!ListAppend((List *) list.Get(), (ArObject *) expr)) {
            Release(expr);
            throw DatatypeException();
        }

        end = expr->loc.end;

        Release(expr);

        this->IgnoreNL();
    } while (this->MatchEat(TokenType::COMMA));

    auto *unary = UnaryNew(list.Get(), NodeType::TUPLE, this->tkcur_.loc);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = left->loc.start;
    unary->loc.end = end;

    return (Node *) unary;
}

Node *Parser::ParseFor() {
    ARC init;
    ARC test;
    ARC inc;
    ARC body;

    Position start = this->tkcur_.loc.start;
    NodeType type = NodeType::FOR;

    this->Eat();
    this->IgnoreNL();

    if (!this->Match(TokenType::SEMICOLON)) {
        if (this->Match(scanner::TokenType::KW_VAR))
            init = (ArObject *) this->ParseVarDecl(false, false, false);
        else
            init = (ArObject *) this->ParseExpression(0);
    }

    this->IgnoreNL();

    if (this->MatchEat(TokenType::KW_OF)) {
        const auto *check = (Node *) init.Get();

        if (check->node_type != NodeType::DECLARATION &&
            check->node_type != NodeType::IDENTIFIER &&
            check->node_type != NodeType::TUPLE)
            throw ParserException("expected var declaration, identifier or tuple before 'of' in foreach");

        if (check->node_type == NodeType::DECLARATION) {
            const auto *decl = (Assignment *) init.Get();

            if (decl->value != nullptr)
                throw ParserException("unexpected initialization of var in foreach");

            if (decl->multi) {
                const auto *tmp = ((List *) decl->name);

                inc = (ArObject *) UnaryNew(decl->name, NodeType::TUPLE, ((Unary *) tmp->objects[0])->loc);
                ((Node *) inc.Get())->loc.end = ((Node *) tmp->objects[tmp->length - 1])->loc.end;
            } else
                inc = IncRef(decl->name);
        } else
            inc = init.Unwrap();

        type = NodeType::FOREACH;
    } else if (!this->MatchEat(TokenType::SEMICOLON))
        throw ParserException("expected ';' after for initialization");

    this->IgnoreNL();

    if (type == NodeType::FOR) {
        test = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::EQUAL));

        this->IgnoreNL();

        if (!this->MatchEat(TokenType::SEMICOLON))
            throw ParserException("expected ';' after test");

        this->IgnoreNL();

        inc = (ArObject *) this->ParseExpression(0);
    } else
        test = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::EQUAL));

    body = (ArObject *) this->ParseBlock(ParserScope::LOOP);

    auto *loop = LoopNew((Node *) init.Get(), (Node *) test.Get(), (Node *) inc.Get(), (Node *) body.Get(), type);
    if (loop == nullptr)
        throw DatatypeException();

    loop->loc.start = start;

    return (Node *) loop;
}

Node *Parser::ParseFn(bool pub) {
    ARC name;
    ARC params;
    ARC body;
    ARC tmp;

    Position start = this->tkcur_.loc.start;

    // eat 'func' keyword
    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        throw ParserException("expected identifier after 'func' keyword");

    name = (ArObject *) StringNew((const char *) this->tkcur_.buffer, this->tkcur_.length);

    this->Eat();

    if (this->MatchEat(TokenType::LEFT_ROUND)) {
        params = this->ParseParamList(false, nullptr);

        this->IgnoreNL();

        if (!this->MatchEat(TokenType::RIGHT_ROUND))
            throw ParserException("expected ')' after function params");
    }

    this->EnterDocContext();

    if (!this->ScopeExactMatch(ParserScope::TRAIT) || this->Match(TokenType::LEFT_BRACES))
        body = (ArObject *) this->ParseBlock(ParserScope::BLOCK);

    auto *func = FunctionNew((String *) name.Get(), (List *) params.Get(), (Node *) body.Get(), pub);
    if (func == nullptr)
        throw DatatypeException();

    func->loc.start = start;
    func->doc = this->doc_string_->Unwrap();

    this->ExitDocContext();

    return (Node *) func;
}

Node *Parser::ParseFnCall(Node *left) {
    ARC arg;
    ARC kwarg;
    ARC list;

    int mode = 0;

    // (
    this->Eat();
    this->IgnoreNL();

    list = (ArObject *) ListNew();
    if (!list)
        throw DatatypeException();

    if (this->Match(TokenType::RIGHT_ROUND)) {
        auto *call = CallNew(left, list.Get(), nullptr);
        if (call == nullptr)
            throw DatatypeException();

        call->loc.end = this->tkcur_.loc.end;
        this->Eat();

        return (Node *) call;
    }

    do {
        this->IgnoreNL();

        if (this->ParseFnCallUnpack(kwarg, mode >= 3)) {
            mode = 3;

            continue;
        }

        arg = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::COMMA));

        if (this->ParseFnCallRestArgs((Node *) arg.Get(), list, mode == 1))
            mode = 1;
        else if (this->ParseFnCallNamedArg((Node *) arg.Get(), kwarg, mode == 2))
            mode = 2;
        else if (!ListAppend((List *) list.Get(), arg.Get()))
            throw DatatypeException();
    } while (this->MatchEat(TokenType::COMMA));

    this->IgnoreNL();

    auto *call = CallNew(left, list.Get(), kwarg.Get());
    if (call == nullptr)
        throw DatatypeException();

    call->loc.end = this->tkcur_.loc.end;

    if (!this->MatchEat(TokenType::RIGHT_ROUND)) {
        Release(call);
        throw ParserException("expected ')' after last argument of function call");
    }

    return (Node *) call;
}

Node *Parser::ParseFromImport(bool pub) {
    // from "x/y/z" import xyz as x
    ARC import_list;
    ARC mname;

    Position start = this->tkcur_.loc.start;
    Position end{};

    this->Eat();
    this->IgnoreNL();

    if (!this->Match(TokenType::STRING))
        throw ParserException("expected module path as string after 'from'");

    mname = (ArObject *) this->ParseLiteral();

    this->IgnoreNL();

    if (!this->MatchEat(TokenType::KW_IMPORT))
        throw ParserException("expected 'import' after module path");

    do {
        ARC id;
        ARC alias;

        this->IgnoreNL();

        if (!this->Match(TokenType::IDENTIFIER)) {
            if (this->MatchEat(TokenType::ASTERISK))
                break;

            throw ParserException("expected name");
        }

        id = (ArObject *) this->ParseIdentifier();

        this->IgnoreNewLineIF(TokenType::KW_AS);

        if (this->MatchEat(TokenType::KW_AS)) {
            if (!this->Match(TokenType::IDENTIFIER))
                throw ParserException("expected alias after 'as' keyword");

            alias = (ArObject *) this->ParseIdentifier();
        }

        auto *binary = BinaryNew((Node *) id.Get(), (Node *) alias.Get(), scanner::TokenType::TK_NULL,
                                 NodeType::IMPORT_NAME);
        if (binary == nullptr)
            throw DatatypeException();

        if (!import_list) {
            import_list = (ArObject *) ListNew();
            if (!import_list)
                throw DatatypeException();
        }

        if (!ListAppend((List *) import_list.Get(), (ArObject *) binary)) {
            Release(binary);
            throw DatatypeException();
        }

        end = binary->loc.end;

        Release(binary);

        this->IgnoreNewLineIF(TokenType::COMMA);
    } while (this->MatchEat(scanner::TokenType::COMMA));

    auto *imp = ImportNew((Node *) mname.Get(), import_list.Get(), pub);
    if (imp == nullptr)
        throw DatatypeException();

    imp->loc.start = start;
    imp->loc.end = end;

    return (Node *) imp;
}

Node *Parser::ParseIdentifier() {
    if (!this->Match(TokenType::IDENTIFIER, TokenType::BLANK, TokenType::SELF))
        throw ParserException("expected identifier");

    auto *id = MakeIdentifier(&this->tkcur_);
    if (id == nullptr)
        throw DatatypeException();

    this->Eat();

    return id;
}

Node *Parser::ParseIDValue(NodeType type, const scanner::Position &start) {
    auto id = (Unary *) MakeIdentifier(&this->tkcur_);
    if (id == nullptr)
        throw DatatypeException();

    auto *param = ArgumentNew(id, nullptr, type);

    Release(id);

    if (param == nullptr)
        throw DatatypeException();

    param->loc.start = start;

    this->Eat();

    return (Node *) param;
}

Node *Parser::ParseIF() {
    ARC test;
    ARC body;
    ARC orelse;

    auto scope = ParserScope::IF;

    if(this->scope_stack_->scope == ParserScope::STRUCT || this->scope_stack_->scope == ParserScope::TRAIT)
        scope = this->scope_stack_->scope;

    Position start = this->tkcur_.loc.start;
    Position end{};

    this->Eat();

    test = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::EQUAL));

    body = (ArObject *) this->ParseBlock(scope);

    end = ((Node *) body.Get())->loc.end;

    if (this->Match(TokenType::KW_ELIF)) {
        orelse = (ArObject *) this->ParseIF();
        end = ((Node *) orelse.Get())->loc.end;
    } else if (this->MatchEat(TokenType::KW_ELSE)) {
        orelse = (ArObject *) this->ParseBlock(scope);
        end = ((Node *) orelse.Get())->loc.end;
    }

    auto *tnode = TestNew((Node *) test.Get(), (Node *) body.Get(), (Node *) orelse.Get(), NodeType::IF);
    if (tnode == nullptr)
        throw DatatypeException();

    tnode->loc.start = start;
    tnode->loc.end = end;

    return (Node *) tnode;
}

Node *Parser::ParseImport(bool pub) {
    ARC path;
    ARC import_list;

    Position start = this->tkcur_.loc.start;
    Position end{};

    this->Eat();

    import_list = (ArObject *) ListNew();
    if (!import_list)
        throw DatatypeException();

    do {
        ARC id;

        this->IgnoreNL();

        if (!this->Match(TokenType::STRING))
            throw ParserException("expected path as string after 'import'");

        path = (ArObject *) this->ParseLiteral();

        this->IgnoreNewLineIF(scanner::TokenType::KW_AS);

        if (this->MatchEat(scanner::TokenType::KW_AS)) {
            this->IgnoreNL();

            id = (ArObject *) this->ParseIdentifier();
        }

        end = this->tkcur_.loc.end;

        auto *binary = BinaryNew((Node *) path.Get(), (Node *) id.Get(), scanner::TokenType::TK_NULL,
                                 NodeType::IMPORT_NAME);
        if (binary == nullptr)
            throw DatatypeException();

        if (!ListAppend((List *) import_list.Get(), (ArObject *) binary)) {
            Release(binary);
            throw DatatypeException();
        }

        Release(binary);

        this->IgnoreNewLineIF(scanner::TokenType::COMMA);
    } while (this->MatchEat(scanner::TokenType::COMMA));

    auto *imp = ImportNew(nullptr, import_list.Get(), pub);
    if (imp == nullptr)
        throw DatatypeException();

    imp->loc.start = start;
    imp->loc.end = end;

    return (Node *) imp;
}

Node *Parser::ParseIn(Node *left) {
    NodeType kind = NodeType::IN;

    if (TKCUR_TYPE == TokenType::KW_NOT) {
        kind = NodeType::NOT_IN;

        this->Eat();
        this->IgnoreNL();
    }

    this->Eat();
    this->IgnoreNL();

    auto *expr = this->ParseExpression(PeekPrecedence(TokenType::KW_IN));

    auto *binary = BinaryNew(left, expr, scanner::TokenType::TK_NULL, kind);
    if (binary == nullptr)
        throw DatatypeException();

    return (Node *) binary;
}

Node *Parser::ParseInfix(Node *left) {
    TokenType kind = TKCUR_TYPE;
    ARC right;

    this->Eat();
    this->IgnoreNL();

    right = (ArObject *) this->ParseExpression(this->PeekPrecedence(kind));

    auto *binary = BinaryNew(left, (Node *) right.Get(), kind, NodeType::BINARY);
    if (binary == nullptr)
        throw DatatypeException();

    return (Node *) binary;
}

Node *Parser::ParseInit(Node *left) {
    ARC list;
    bool kwargs = false;
    int count = 0;

    this->Eat();
    this->IgnoreNL();

    list = (ArObject *) ListNew();
    if (!list)
        throw DatatypeException();

    if (this->Match(TokenType::RIGHT_ROUND)) {
        auto *init = InitNew(left, nullptr, this->tkcur_.loc, false);
        if (init == nullptr)
            throw DatatypeException();

        this->Eat();

        return (Node *) init;
    }

    do {
        this->IgnoreNL();

        auto *key = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

        if (!ListAppend((List *) list.Get(), (ArObject *) key)) {
            Release(key);
            throw DatatypeException();
        }

        Release(key);

        this->IgnoreNL();

        count++;
        if (this->MatchEat(TokenType::EQUAL)) {
            if (key->node_type != NodeType::IDENTIFIER)
                throw ParserException("invalid initialization key");

            if (--count != 0)
                throw ParserException("can't mix field names with positional initialization");

            auto *value = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

            if (!ListAppend((List *) list.Get(), (ArObject *) value)) {
                Release(value);
                throw DatatypeException();
            }

            Release(value);

            kwargs = true;

            this->IgnoreNL();

            continue;
        }

        if (kwargs)
            throw ParserException("can't mix positional with field names initialization");

        this->IgnoreNL();
    } while (this->MatchEat(TokenType::COMMA));

    auto *init = InitNew(left, list.Get(), this->tkcur_.loc, count == 0);
    if (init == nullptr)
        throw DatatypeException();

    if (!this->MatchEat(TokenType::RIGHT_ROUND)) {
        Release(init);
        throw ParserException("expected ')' after struct initialization");
    }

    return (Node *) init;
}

Node *Parser::ParseList() {
    Position start = this->tkcur_.loc.start;
    ARC list;

    this->Eat();
    this->IgnoreNL();

    list = (ArObject *) ListNew();
    if (!list)
        throw DatatypeException();

    if (!this->Match(TokenType::RIGHT_SQUARE)) {
        do {
            this->IgnoreNL();

            auto *itm = this->ParseExpression(PeekPrecedence(TokenType::COMMA));

            if (!ListAppend((List *) list.Get(), (ArObject *) itm)) {
                Release(itm);
                throw DatatypeException();
            }

            Release(itm);

            this->IgnoreNL();
        } while (this->MatchEat(TokenType::COMMA));
    }

    auto *unary = UnaryNew(list.Get(), NodeType::LIST, this->tkcur_.loc);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;

    if (!this->MatchEat(TokenType::RIGHT_SQUARE)) {
        Release(unary);
        throw ParserException("expected ']' after list definition");
    }

    return (Node *) unary;
}

Node *Parser::ParseLiteral() {
    ARC value;
    Node *literal;

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

    this->Eat();

    if (!value)
        throw DatatypeException();

    if ((literal = (Node *) UnaryNew(value.Get(), NodeType::LITERAL, this->tkcur_.loc)) == nullptr)
        throw DatatypeException();

    return literal;
}

Node *Parser::ParseLoop() {
    ARC test;
    ARC body;

    Position start = this->tkcur_.loc.start;

    this->Eat();

    if (!this->Match(scanner::TokenType::LEFT_BRACES))
        test = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::EQUAL));

    body = (ArObject *) this->ParseBlock(ParserScope::LOOP);

    auto *loop = LoopNew(nullptr, (Node *) test.Get(), nullptr, (Node *) body.Get(), NodeType::LOOP);
    if (loop == nullptr)
        throw DatatypeException();

    loop->loc.start = start;

    return (Node *) loop;
}

Node *Parser::ParseIDNamedParam(bool parse_expr) {
    ARC id;
    ARC value;

    Loc loc{};

    if (parse_expr)
        id = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::COMMA));
    else
        id = (ArObject *) this->Unknown2Identifier();

    loc = this->tkcur_.loc;

    if (this->MatchEat(scanner::TokenType::EQUAL)) {
        if (((Node *) id.Get())->node_type != NodeType::IDENTIFIER)
            throw ParserException("expected identifier before = in named parameter declaration");

        if (this->Match(TokenType::COMMA, TokenType::RIGHT_ROUND)) {
            value = (ArObject *) UnaryNew((ArObject *) Nil, NodeType::LITERAL, loc);
            if (!value)
                throw DatatypeException();
        } else
            value = (ArObject *) this->ParseExpression(PeekPrecedence(scanner::TokenType::COMMA));
    }

    if ((((Node *) id.Get())->node_type == NodeType::IDENTIFIER)) {
        auto *param = ArgumentNew((Unary *) id.Get(), (Node *) value.Get(), NodeType::ARGUMENT);
        if (param == nullptr)
            throw DatatypeException();

        return (Node *) param;
    }

    return (Node *) id.Unwrap();
}

Node *Parser::ParseNullCoalescing(Node *left) {
    this->Eat();
    this->IgnoreNL();

    auto *expr = this->ParseExpression(PeekPrecedence(scanner::TokenType::NULL_COALESCING));

    auto *binary = BinaryNew(left, expr, TokenType::TK_NULL, NodeType::NULL_COALESCING);
    if (binary == nullptr) {
        Release(expr);
        throw DatatypeException();
    }

    return (Node *) binary;
}

Node *Parser::ParseOOBCall() {
    Position start = this->tkcur_.loc.start;
    TokenType type = TKCUR_TYPE;

    this->Eat();
    this->IgnoreNL();

    auto *expr = this->ParseExpression(0);

    if (expr->node_type != NodeType::CALL) {
        Release(expr);

        if (type == scanner::TokenType::KW_DEFER)
            throw ParserException("defer expected call expression");
        else if (type == scanner::TokenType::KW_SPAWN)
            throw ParserException("spawn expected call expression");
        else
            assert(false);
    }

    expr->loc.start = start;
    expr->token_type = type;

    return expr;
}

Node *Parser::ParsePipeline(Node *left) {
    Node *right;

    this->Eat();
    this->IgnoreNL();

    right = this->ParseExpression(PeekPrecedence(TokenType::ASTERISK));
    if (right->node_type == NodeType::CALL) {
        auto *call = (Call *) right;

        if (!ListPrepend((List *) call->args, (ArObject *) left)) {
            Release(right);
            throw DatatypeException();
        }

        right->loc.start = left->loc.start;

        return right;
    }

    auto *args = ListNew();
    if (args == nullptr) {
        Release(right);
        throw DatatypeException();
    }

    if (!ListAppend(args, (ArObject *) left)) {
        Release(right);
        Release(args);
        throw DatatypeException();
    }

    auto *call = CallNew(right, (ArObject *) args, nullptr);
    Release(right);
    Release(args);

    if (call == nullptr)
        throw DatatypeException();

    return (Node *) call;
}

Node *Parser::ParsePostInc(Node *left) {
    if (left->node_type != NodeType::IDENTIFIER &&
        left->node_type != NodeType::INDEX &&
        left->node_type != NodeType::SELECTOR)
        throw ParserException("unexpected update operator");

    auto *unary = UnaryNew((ArObject *) left, NodeType::UPDATE, this->tkcur_.loc);
    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = left->loc.start;
    unary->loc.end = this->tkcur_.loc.end;

    unary->token_type = TKCUR_TYPE;

    this->Eat();

    return (Node *) unary;
}

Node *Parser::ParsePrefix() {
    Position start = this->tkcur_.loc.start;
    TokenType kind = TKCUR_TYPE;
    Unary *unary;

    this->Eat();

    auto *right = this->ParseExpression(PeekPrecedence(kind));

    unary = UnaryNew((ArObject *) right, kind, right->loc);

    Release(right);

    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;

    return (Node *) unary;
}

Node *Parser::ParseScope() {
    ARC ident;

    if (!this->Match(TokenType::IDENTIFIER))
        throw ParserException("expected identifier");

    ident = (ArObject *) this->ParseIdentifier();

    this->IgnoreNL();
    while (this->Match(TokenType::SCOPE, TokenType::DOT)) {
        ident = (ArObject *) this->ParseSelector((Node *) ident.Get());
        this->IgnoreNL();
    }

    return (Node *) ident.Unwrap();
}

Node *Parser::ParseSelector(Node *left) {
    TokenType kind = TKCUR_TYPE;

    this->Eat();
    this->IgnoreNL();

    if (!this->Match(TokenType::IDENTIFIER))
        throw ParserException("expected identifier after '.'/'?.'/'::' access operator");

    auto *right = this->ParseIdentifier();

    auto *binary = BinaryNew(left, right, kind, NodeType::SELECTOR);

    Release(right);

    if (binary == nullptr)
        throw DatatypeException();

    return (Node *) binary;
}

Node *Parser::ParseStatement() {
    ARC expr;
    ARC label;

    do {
        switch (TKCUR_TYPE) {
            case TokenType::KW_ASSERT:
                expr = (ArObject *) this->ParseAssertion();
                break;
            case TokenType::KW_DEFER:
            case TokenType::KW_SPAWN:
                expr = (ArObject *) this->ParseOOBCall();
                break;
            case TokenType::KW_RETURN:
                expr = (ArObject *) this->ParseUnaryStmt(NodeType::RETURN, false);
                break;
            case TokenType::KW_YIELD:
                expr = (ArObject *) this->ParseUnaryStmt(NodeType::YIELD, true);
                break;
            case TokenType::KW_FOR:
                expr = (ArObject *) this->ParseFor();
                break;
            case TokenType::KW_LOOP:
                expr = (ArObject *) this->ParseLoop();
                break;
            case TokenType::KW_PANIC:
                expr = (ArObject *) this->ParseUnaryStmt(NodeType::PANIC, true);
                break;
            case TokenType::KW_IF:
                expr = (ArObject *) this->ParseIF();
                break;
            case TokenType::KW_SWITCH:
                expr = (ArObject *) this->ParseSwitch();
                break;
            case TokenType::KW_BREAK:
                if (!this->ScopeMatch(ParserScope::LOOP) && !this->ScopeMatch(ParserScope::SWITCH))
                    throw ParserException("'break' not allowed outside loop or switch");

                expr = (ArObject *) this->ParseBCFLabel();
                break;
            case TokenType::KW_CONTINUE:
                if (!this->ScopeMatch(ParserScope::LOOP))
                    throw ParserException("'continue' not allowed outside of loop");

                expr = (ArObject *) this->ParseBCFLabel();
                break;
            case TokenType::KW_FALLTHROUGH:
                if (!this->ScopeExactMatch(ParserScope::SWITCH))
                    throw ParserException("'fallthrough' not allowed outside of switch");

                expr = (ArObject *) this->ParseBCFLabel();
                break;
            default:
                expr = (ArObject *) this->ParseExpression();
        }

        if (((Node *) expr.Get())->node_type != NodeType::IDENTIFIER || !this->MatchEat(TokenType::COLON))
            break;

        this->Eat();
        this->IgnoreNL();

        if (label)
            throw ParserException("expected statement after label");

        label = expr;
    } while (true);

    if (label) {
        auto *check = (Node *) expr.Get();

        if (check->node_type != NodeType::FOR &&
            check->node_type != NodeType::FOREACH &&
            check->node_type != NodeType::LOOP)
            throw ParserException("expected for/loop after label");

        auto *lbl = BinaryNew((Node *) label.Get(), check, TokenType::TK_NULL, NodeType::LABEL);
        if (lbl == nullptr)
            throw DatatypeException();

        return (Node *) lbl;
    }

    return (Node *) expr.Unwrap();
}

Node *Parser::ParseStructDecl(bool pub) {
    ARC name;
    ARC impls;
    ARC block;

    Position start = this->tkcur_.loc.start;

    // eat 'struct' keyword
    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        throw ParserException("expected identifier after 'struct' keyword");

    name = (ArObject *) StringNew((const char *) this->tkcur_.buffer, this->tkcur_.length);

    this->Eat();
    this->IgnoreNL();

    if (this->MatchEat(TokenType::KW_IMPL)) {
        this->IgnoreNL();

        impls = this->ParseTraitList();

        this->IgnoreNL();
    }

    this->EnterDocContext();

    block = (ArObject *) this->ParseBlock(ParserScope::STRUCT);

    auto *cstr = ConstructNew((String *) name.Get(), (List *) impls.Get(), (Node *) block.Get(), NodeType::STRUCT, pub);
    if (cstr == nullptr)
        throw DatatypeException();

    cstr->loc.start = start;
    cstr->doc = this->doc_string_->Unwrap();

    this->ExitDocContext();

    return (Node *) cstr;
}

Node *Parser::ParseSubscript(Node *left) {
    ARC start;
    ARC stop;

    bool is_slice = false;

    this->Eat();
    this->IgnoreNL();

    if (this->Match(TokenType::RIGHT_SQUARE))
        throw ParserException("subscript definition (index | slice) cannot be empty");

    if (!this->Match(TokenType::COLON))
        start = (ArObject *) this->ParseExpression(0);

    this->IgnoreNL();

    if (this->MatchEat(TokenType::COLON)) {
        this->IgnoreNL();

        if (!this->Match(TokenType::RIGHT_SQUARE))
            stop = (ArObject *) this->ParseExpression(0);

        is_slice = true;
    }

    auto *slice = SubscriptNew(left, (Node *) start.Get(), (Node *) stop.Get(), is_slice);
    if (slice == nullptr)
        throw DatatypeException();

    slice->loc.end = this->tkcur_.loc.end;

    this->IgnoreNL();

    if (!this->MatchEat(TokenType::RIGHT_SQUARE)) {
        Release(slice);
        throw ParserException(stop ?
                              "expected ']' after slice definition" :
                              "expected ']' after index definition");
    }

    return (Node *) slice;
}

Node *Parser::ParseSwitch() {
    ARC cases;
    ARC test;

    bool def = false;

    Position start = this->tkcur_.loc.start;

    this->Eat();

    if (!this->Match(TokenType::LEFT_BRACES))
        test = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::EQUAL));

    if (!this->MatchEat(TokenType::LEFT_BRACES))
        throw ParserException("expected '{' after switch declaration");

    this->IgnoreNL();

    cases = (ArObject *) ListNew();
    if (!cases)
        throw DatatypeException();

    while (this->Match(scanner::TokenType::KW_CASE, TokenType::KW_DEFAULT)) {
        if (this->Match(scanner::TokenType::KW_DEFAULT)) {
            if (def)
                throw ParserException("default case already defined");

            def = true;
        }

        auto *cs = this->ParseSwitchCase();

        if (!ListAppend((List *) cases.Get(), (ArObject *) cs)) {
            Release(cs);
            throw DatatypeException();
        }

        Release(cs);
        this->IgnoreNL();
    }

    auto *sw = TestNew((Node *) test.Get(), (Node *) cases.Get(), nullptr, NodeType::SWITCH);
    if (sw == nullptr)
        throw DatatypeException();

    sw->loc = this->tkcur_.loc;
    sw->loc.start = start;

    if (!this->MatchEat(TokenType::RIGHT_BRACES))
        throw ParserException("expected '}' after switch declaration");

    return (Node *) sw;
}

Node *Parser::ParseSwitchCase() {
    Loc loc = this->tkcur_.loc;
    ARC conditions;
    ARC body;

    if (this->MatchEat(scanner::TokenType::KW_CASE)) {
        conditions = (ArObject *) ListNew();
        if (!conditions)
            throw DatatypeException();

        do {
            this->IgnoreNL();

            auto *cond = this->ParseExpression(PeekPrecedence(scanner::TokenType::PIPELINE) - 1);

            if (!ListAppend((List *) conditions.Get(), (ArObject *) cond)) {
                Release(cond);
                throw DatatypeException();
            }

            Release(cond);

            this->IgnoreNL();
        } while (this->MatchEat(scanner::TokenType::SEMICOLON));
    } else if (!this->MatchEat(scanner::TokenType::KW_DEFAULT))
        throw ParserException("expected 'case' or 'default' label");

    if (!this->MatchEat(scanner::TokenType::COLON)) {
        if (!conditions)
            throw ParserException("expected ':' after 'default' label");

        throw ParserException("expected ':' after 'case' label");
    }

    loc.end = this->tkcur_.loc.end;

    this->IgnoreNL();

    while (!this->Match(TokenType::KW_CASE, TokenType::KW_DEFAULT, TokenType::RIGHT_BRACES)) {
        if (!body) {
            body = (ArObject *) ListNew();
            if (!body)
                throw DatatypeException();
        }

        auto *decl = this->ParseDecls(ParserScope::SWITCH);

        if (!ListAppend((List *) body.Get(), (ArObject *) decl)) {
            Release(decl);
            throw DatatypeException();
        }

        Release(decl);

        loc.end = decl->loc.end;

        this->IgnoreNL();
    }

    auto sc = SwitchCaseNew(conditions.Get(), body.Get(), loc);
    if (sc == nullptr)
        throw DatatypeException();

    return (Node *) sc;
}

Node *Parser::ParseSyncBlock() {
    ARC lock_expr;
    ARC body;

    Position start = this->tkcur_.loc.start;

    this->Eat();

    lock_expr = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::ASTERISK));

    if (((Node *) lock_expr.Get())->node_type == NodeType::LITERAL)
        throw ParserException("sync block requires an object reference, not a literal");

    body = (ArObject *) this->ParseBlock(ParserScope::SYNCBLOCK);

    auto binary = BinaryNew((Node *) lock_expr.Get(), (Node *) body.Get(), scanner::TokenType::TK_NULL,
                            NodeType::SYNC_BLOCK);
    if (binary == nullptr)
        throw DatatypeException();

    binary->loc.start = start;

    return (Node *) binary;
}

Node *Parser::ParseTernary(Node *left) {
    ARC body;
    ARC orelse;

    this->Eat();
    this->IgnoreNL();

    body = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::COMMA));

    this->IgnoreNL();

    if (this->MatchEat(TokenType::COLON)) {
        this->IgnoreNL();

        orelse = (ArObject *) this->ParseExpression(PeekPrecedence(TokenType::COMMA));
    }

    auto *test = TestNew(left, (Node *) body.Get(), (Node *) orelse.Get(), NodeType::TERNARY);
    if (test == nullptr)
        throw DatatypeException();

    return (Node *) test;
}

Node *Parser::ParseTraitDecl(bool pub) {
    ARC name;
    ARC impls;
    ARC block;

    Position start = this->tkcur_.loc.start;

    // eat 'trait' keyword
    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        throw ParserException("expected identifier after 'trait' keyword");

    name = (ArObject *) StringNew((const char *) this->tkcur_.buffer, this->tkcur_.length);

    this->Eat();
    this->IgnoreNL();

    if (this->MatchEat(TokenType::COLON)) {
        this->IgnoreNL();

        impls = this->ParseTraitList();

        this->IgnoreNL();
    }

    this->EnterDocContext();

    block = (ArObject *) this->ParseBlock(ParserScope::TRAIT);

    auto *cstr = ConstructNew((String *) name.Get(), (List *) impls.Get(), (Node *) block.Get(), NodeType::TRAIT, pub);
    if (cstr == nullptr)
        throw DatatypeException();

    cstr->loc.start = start;
    cstr->doc = this->doc_string_->Unwrap();

    this->ExitDocContext();

    return (Node *) cstr;
}

Node *Parser::ParseTrap() {
    Position start = this->tkcur_.loc.start;

    this->Eat();
    this->IgnoreNL();

    auto *trap_expr = this->ParseExpression(PeekPrecedence(TokenType::EQUAL));

    if (trap_expr->node_type == NodeType::TRAP) {
        Release(trap_expr);
        throw ParserException("invalid use of trap, trap does not intercept another trap");
    }

    auto *unary = UnaryNew((ArObject *) trap_expr, NodeType::TRAP, trap_expr->loc);

    unary->loc.start = start;

    return (Node *) unary;
}

Node *Parser::ParseVarDecl(bool visibility, bool constant, bool weak) {
    ARC assign;
    Token token;

    this->Eat();
    this->IgnoreNL();

    if (!this->Match(TokenType::IDENTIFIER)) {
        throw ParserException(constant ? "expected identifier after let keyword"
                                       : "expected identifier after var keyword");
    }

    token = std::move(this->tkcur_);

    this->Eat();
    this->IgnoreNewLineIF(TokenType::COMMA, TokenType::EQUAL);

    if (!this->MatchEat(TokenType::COMMA)) {
        auto *id = MakeIdentifier(&token);
        if (id == nullptr)
            throw DatatypeException();

        assign = (ArObject *) AssignmentNew((ArObject *) id, constant, visibility, weak);
        Release(id);

        if (!assign)
            throw DatatypeException();

        ((Node *) assign.Get())->loc = token.loc;
    } else
        assign = (ArObject *) this->ParseVarDeclTuple(token, visibility, constant, weak);

    this->IgnoreNewLineIF(TokenType::EQUAL);

    if (this->MatchEat(TokenType::EQUAL)) {
        this->IgnoreNL();

        auto *values = this->ParseExpression(PeekPrecedence(TokenType::EQUAL));
        auto *as = (Assignment *) assign.Get();

        as->loc.end = values->loc.end;
        as->value = (ArObject *) values;
    } else if (constant)
        throw ParserException("expected = after identifier/s in let declaration");

    return (Node *) assign.Unwrap();
}

Node *Parser::ParseVarDeclTuple(const Token &token, bool visibility, bool constant, bool weak) {
    ARC id;
    ARC ids;

    Position end{};

    ids = (ArObject *) ListNew();
    if (!ids)
        throw DatatypeException();

    id = (ArObject *) MakeIdentifier(&token);
    if (!id)
        throw DatatypeException();

    if (!ListAppend((List *) ids.Get(), id.Get()))
        throw DatatypeException();

    do {
        this->IgnoreNL();

        id = (ArObject *) this->ParseIdentifier();
        if (!ListAppend((List *) ids.Get(), id.Get()))
            throw DatatypeException();

        end = this->tkcur_.loc.end;

        this->IgnoreNL();
    } while (this->MatchEat(TokenType::COMMA));

    auto *assign = AssignmentNew(ids.Get(), constant, visibility, weak);
    if (assign == nullptr)
        throw DatatypeException();

    assign->loc.start = token.loc.start;
    assign->loc.end = end;

    return (Node *) assign;
}

Node *Parser::ParseUnaryStmt(NodeType type, bool expr_required) {
    Loc loc = this->tkcur_.loc;
    Node *expr = nullptr;

    this->Eat();
    this->IgnoreNL();

    if (!this->Match(scanner::TokenType::END_OF_FILE, TokenType::RIGHT_BRACES, TokenType::SEMICOLON))
        expr = this->ParseExpression(PeekPrecedence(TokenType::EQUAL));
    else if (expr_required)
        throw ParserException("expected expression");

    auto *unary = UnaryNew((ArObject *) expr, type, loc);
    if (unary == nullptr) {
        Release(expr);
        throw DatatypeException();
    }

    if (expr != nullptr)
        unary->loc.end = expr->loc.end;

    Release(expr);

    return (Node *) unary;
}

Node *Parser::ParseWarlus(Node *left) {
    ArObject *tmp;
    Node *right;

    this->Eat();
    this->IgnoreNL();

    // Check left
    if (left->node_type == NodeType::IDENTIFIER)
        tmp = (ArObject *) left;
    else if (left->node_type == NodeType::TUPLE) {
        auto *list = (List *) ((Unary *) left)->value;

        for (ArSize i = 0; i < list->length; i++) {
            if (((Node *) list->objects[i])->node_type != NodeType::IDENTIFIER)
                throw ParserException("expected only identifiers before := operator");
        }

        tmp = (ArObject *) list;
    } else
        throw ParserException("expected identifier/s before := operator");

    right = this->ParseExpression(PeekPrecedence(TokenType::WALRUS));

    auto assign = AssignmentNew(tmp, false, false, false);
    if (assign == nullptr)
        throw DatatypeException();

    assign->value = (ArObject *) right;

    return (Node *) assign;
}

Node *Parser::Unknown2Identifier() {
    bool literal = true;

    for (ArSize i = 0; i < this->tkcur_.length; i++) {
        if (!isalnum(this->tkcur_.buffer[i]) && this->tkcur_.buffer[i] != '_') {
            literal = false;
            break;
        }
    }

    if (!literal)
        throw ParserException("invalid token");

    auto *id = MakeIdentifier(&this->tkcur_);
    if (id == nullptr)
        throw DatatypeException();

    this->Eat();

    return id;
}

void Parser::Eat() {
    if (this->tkcur_.type == TokenType::END_OF_FILE)
        return;

    do {
        if (!this->scanner_.NextToken(&this->tkcur_))
            throw ScannerException();
        if (this->doc_string_->uninterrupted &&
            this->TokenInRange(TokenType::COMMENT_BEGIN, TokenType::COMMENT_END)) {
            this->doc_string_->AddString(this->tkcur_);
        } else if (!this->Match(scanner::TokenType::END_OF_LINE))
            this->doc_string_->uninterrupted = false;

    } while (this->TokenInRange(scanner::TokenType::COMMENT_BEGIN, scanner::TokenType::COMMENT_END));
}

void Parser::EnterDocContext() {
    auto *ds = DocStringNew(this->doc_string_);
    if (ds == nullptr)
        throw DatatypeException();

    this->doc_string_ = ds;
}

void Parser::IgnoreNL() {
    while (this->Match(TokenType::END_OF_LINE))
        this->Eat();
}

void Parser::ScopePush(ScopeEntry *entry) {
    entry->prev = this->scope_stack_;
    this->scope_stack_ = entry;
}

void Parser::ScopePop() {
    if (this->scope_stack_ != nullptr)
        this->scope_stack_ = this->scope_stack_->prev;
}

// PUBLIC

File *Parser::Parse() noexcept {
    ARC result;
    ARC doc;

    Position start{};
    Position end{};

    List *statements;

    if ((statements = ListNew()) == nullptr)
        return nullptr;

    try {
        this->EnterDocContext();

        this->Eat();
        this->IgnoreNL();

        start = this->tkcur_.loc.start;

        while (!this->Match(TokenType::END_OF_FILE)) {
            result = (ArObject *) this->ParseDecls(ParserScope::MODULE);

            if (!ListAppend(statements, result.Get())) {
                Release(statements);
                return nullptr;
            }

            end = this->tkcur_.loc.end;

            if (!this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON, TokenType::END_OF_FILE))
                throw ParserException("invalid token at the end of the line");

            while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON))
                this->Eat();
        }

        doc = (ArObject *) this->doc_string_->Unwrap();

        this->ExitDocContext();
    } catch (const ScannerException &) {
        ErrorFormat(kParserErrors[0], "%s", this->scanner_.GetStatusMessage());
        return nullptr;
    } catch (const ParserException &e) {
        ErrorFormat(kParserErrors[1], "%s", e.what());
        return nullptr;
    } catch (const DatatypeException &) {
        // This exception can be safely ignored!
        return nullptr;
    }

    auto *file = FileNew(this->filename_, statements);
    if (file != nullptr) {
        file->loc.start = start;
        file->loc.end = end;
        file->doc = (String *) doc.Unwrap();
    }

    Release(statements);

    return file;
}
