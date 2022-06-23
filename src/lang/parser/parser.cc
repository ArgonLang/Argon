// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0
// 05/12/2019 <3

#include <object/datatype/bool.h>
#include <object/datatype/bytes.h>
#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/list.h>
#include <object/datatype/nil.h>
#include <object/datatype/string.h>

#include "parser.h"
#include "object/datatype/atom.h"

#define EXPR_NO_ASSIGN          11
#define EXPR_NO_LIST            21

using namespace argon::object;
using namespace argon::lang::scanner;
using namespace argon::lang::parser;

bool IsIdentifiersList(Node *node) {
    auto *ast_list = (Unary *) node;
    auto *list = (List *) ast_list->value;

    if (!AR_TYPEOF(node, type_ast_tuple_))
        return false;

    // All items into list must be IDENTIFIER
    for (int i = 0; i < list->len; i++) {
        if (!AR_TYPEOF(list->objects[i], type_ast_identifier_))
            return false;
    }

    return true;
}

int PeekPrecedence(TokenType type) {
    switch (type) {
        case TokenType::EQUAL:
        case TokenType::PLUS_EQ:
        case TokenType::MINUS_EQ:
        case TokenType::ASTERISK_EQ:
        case TokenType::SLASH_EQ:
            return 10;
        case TokenType::COMMA:
            return 20;
        case TokenType::LEFT_BRACES:
            return 30;
        case TokenType::ELVIS:
        case TokenType::QUESTION:
            return 40;
        case TokenType::PIPELINE:
            return 50;
        case TokenType::OR:
            return 60;
        case TokenType::AND:
            return 70;
        case TokenType::PIPE:
            return 80;
        case TokenType::CARET:
            return 90;
        case TokenType::EQUAL_EQUAL:
        case TokenType::EQUAL_STRICT:
        case TokenType::NOT_EQUAL:
        case TokenType::NOT_EQUAL_STRICT:
            return 100;
        case TokenType::LESS:
        case TokenType::LESS_EQ:
        case TokenType::GREATER:
        case TokenType::GREATER_EQ:
            return 110;
        case TokenType::SHL:
        case TokenType::SHR:
            return 120;
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
            return 130;
        case TokenType::ASTERISK:
        case TokenType::SLASH:
        case TokenType::SLASH_SLASH:
        case TokenType::PERCENT:
            return 140;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
        case TokenType::LEFT_SQUARE:
        case TokenType::LEFT_ROUND:
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
            return 150;
        default:
            return 1000;
    }
}

Node *MakeIdentifier(const Token &token) {
    auto *id = ArObjectNew<Unary>(RCType::INLINE, type_ast_identifier_);
    String *str;

    if (id == nullptr)
        return nullptr;

    if ((str = StringNew((const char *) token.buf)) == nullptr) {
        Release(id);
        return nullptr;
    }

    id->start = token.start;
    id->end = token.end;
    id->kind = token.type;
    id->colno = 0;
    id->lineno = 0;
    id->value = str;

    return id;
}

inline bool Parser::IsLiteral() {
    return this->TokenInRange(TokenType::NUMBER_BEGIN, TokenType::NUMBER_END)
           || this->TokenInRange(TokenType::STRING_BEGIN, TokenType::STRING_END)
           || this->Match(TokenType::ATOM)
           || this->Match(TokenType::TRUE)
           || this->Match(TokenType::FALSE)
           || this->Match(TokenType::NIL);
}

bool Parser::MatchEat(TokenType type, bool eat_nl) {
    bool match;

    if (eat_nl)
        this->EatTerm(type);

    if ((match = this->Match(type))) {
        this->Eat();

        if (eat_nl)
            this->EatTerm(TokenType::END_OF_FILE);
    }

    return match;
}

bool Parser::TokenInRange(TokenType begin, TokenType end) const {
    return this->tkcur_.type > begin && this->tkcur_.type < end;
}

bool Parser::ParseRestElement(Node **rest_node) {
    Pos start = this->tkcur_.start;
    ArObject *str;
    Unary *id;

    if (this->MatchEat(TokenType::ELLIPSIS, false)) {
        if (!this->Match(TokenType::IDENTIFIER)) {
            (*rest_node) = nullptr;
            ErrorFormat(type_syntax_error_, "expected identifier after ...(ellipsis) operator");
            return false;
        }

        if ((id = ArObjectNew<Unary>(RCType::INLINE, type_ast_restid_)) != nullptr) {
            if ((str = StringNew((const char *) this->tkcur_.buf)) == nullptr) {
                Release(id);
                return false;
            }

            id->start = start;
            id->end = this->tkcur_.end;
            id->colno = 0;
            id->lineno = 0;
            id->value = str;

            *rest_node = id;

            this->Eat();
            return true;
        }
    }

    return false;
}

LedMeth Parser::LookupLed() const {
    switch (this->tkcur_.type) {
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            return &Parser::ParsePostUpdate;
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
        case TokenType::SCOPE:
            return &Parser::ParseMemberAccess;
        case TokenType::LEFT_SQUARE:
            return &Parser::ParseSubscript;
        case TokenType::LEFT_BRACES:
            return &Parser::ParseInitialization;
        case TokenType::LEFT_ROUND:
            return &Parser::ParseFnCall;
        case TokenType::PIPELINE:
            return &Parser::ParsePipeline;
        case TokenType::ELVIS:
            return &Parser::ParseElvis;
        case TokenType::QUESTION:
            return &Parser::ParseTernary;
        case TokenType::EQUAL:
        case TokenType::PLUS_EQ:
        case TokenType::MINUS_EQ:
        case TokenType::ASTERISK_EQ:
        case TokenType::SLASH_EQ:
            return &Parser::ParseAssignment;
        case TokenType::COMMA:
            return &Parser::ParseExprList;
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::ASTERISK:
        case TokenType::SLASH:
        case TokenType::SLASH_SLASH:
        case TokenType::PERCENT:
        case TokenType::SHL:
        case TokenType::SHR:
        case TokenType::LESS:
        case TokenType::LESS_EQ:
        case TokenType::GREATER:
        case TokenType::GREATER_EQ:
        case TokenType::EQUAL_EQUAL:
        case TokenType::EQUAL_STRICT:
        case TokenType::NOT_EQUAL_STRICT:
        case TokenType::NOT_EQUAL:
        case TokenType::AMPERSAND:
        case TokenType::CARET:
        case TokenType::PIPE:
        case TokenType::AND:
        case TokenType::OR:
            return &Parser::ParseInfix;
        default:
            return nullptr;
    }
}

ArObject *Parser::ParseArgsKwargs(const char *partial_error, const char *error, bool *is_kwargs) {
    int count = 0;
    List *list;
    Node *tmp;

    if ((list = ListNew()) == nullptr)
        return nullptr;

    *is_kwargs = false;

    this->EatTerm();

    if (!this->Match(TokenType::RIGHT_BRACES)) {
        do {
            count++;
            while (true) {
                if ((tmp = this->ParseExpr(EXPR_NO_LIST)) == nullptr)
                    goto ERROR;

                if (!ListAppend(list, tmp))
                    goto ERROR;

                Release(tmp);

                if (!this->MatchEat(TokenType::COLON, true))
                    break;

                *is_kwargs = true;
                if (--count != 0) {
                    Release(list);
                    return (Node *) ErrorFormat(type_syntax_error_, partial_error);
                }
            }
        } while (this->MatchEat(TokenType::COMMA, true));
    }

    if (count != 0 && (*is_kwargs)) {
        Release(list);
        return (Node *) ErrorFormat(type_syntax_error_, error);
    }

    return list;

    ERROR:
    Release(tmp);
    Release(list);
    return nullptr;
}

argon::object::ArObject *Parser::TraitList() {
    List *traits;
    Node *tmp;

    if ((traits = ListNew()) == nullptr)
        return nullptr;

    do {
        if ((tmp = this->ParseScope()) == nullptr) {
            Release(traits);
            return nullptr;
        }

        if (!ListAppend(traits, tmp)) {
            Release(tmp);
            Release(traits);
            return nullptr;
        }

        Release(tmp);
    } while (this->MatchEat(TokenType::COMMA, true));

    return traits;
}

ArObject *Parser::ScopeAsNameList(bool id_only, bool with_alias) {
    List *imports = nullptr;
    Node *imp;

    if ((imp = this->ScopeAsName(id_only, with_alias)) == nullptr)
        return nullptr;

    if (this->Match(TokenType::COMMA)) {
        if ((imports = ListNew()) == nullptr) {
            Release(imp);
            return nullptr;
        }

        if (!ListAppend(imports, imp)) {
            Release(imp);
            Release(imports);
            return nullptr;
        }

        Release(imp);

        while (this->MatchEat(TokenType::COMMA, false)) {
            if ((imp = this->ScopeAsName(id_only, with_alias)) == nullptr) {
                Release(imports);
                return nullptr;
            }

            if (!ListAppend(imports, imp)) {
                Release(imp);
                Release(imports);
                return nullptr;
            }

            Release(imp);
        }
    }

    if (imports != nullptr)
        return imports;

    return imp;
}

Node *Parser::ParseAssertion() {
    Pos start = this->tkcur_.start;
    Node *msg = nullptr;
    Node *expr;
    Node *ret;

    this->Eat();

    if ((expr = this->ParseExpr(EXPR_NO_LIST)) == nullptr)
        return nullptr;

    if (this->MatchEat(TokenType::COMMA, true)) {
        msg = this->ParseExpr();
        if (msg == nullptr) {
            Release(expr);
            return nullptr;
        }
    }

    if ((ret = BinaryNew(TokenType::ASSERT, type_ast_assert_, expr, msg)) == nullptr) {
        Release(expr);
        Release(msg);
        return nullptr;
    }

    ret->start = start;

    return ret;
}

Node *Parser::ParseAssignment(Node *left) {
    TokenType kind = this->tkcur_.type;
    Node *right;
    Binary *ret;

    this->Eat();

    if (!AR_TYPEOF(left, type_ast_identifier_)
        && !AR_TYPEOF(left, type_ast_index_)
        && !AR_TYPEOF(left, type_ast_tuple_)
        && !AR_TYPEOF(left, type_ast_selector_))
        return (Node *) ErrorFormat(type_syntax_error_,
                                    "expected identifier or list to the left of the assignment expression");

    if (AR_TYPEOF(left, type_ast_tuple_)) {
        auto *list = (List *) ((Unary *) left)->value;

        for (int i = 0; i < list->len; i++) {
            if (!AR_TYPEOF(list->objects[i], type_ast_identifier_) &&
                !AR_TYPEOF(list->objects[i], type_ast_index_) &&
                !AR_TYPEOF(list->objects[i], type_ast_selector_)) {
                Release(left);
                return (Node *) ErrorFormat(type_syntax_error_,
                                            "expected identifier/selector/index as %d element in assignment definition",
                                            i);
            }
        }
    }

    if ((right = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr) {
        Release(left);
        return nullptr;
    }

    if ((ret = BinaryNew(kind, type_ast_assignment_, left, right)) == nullptr) {
        Release(left);
        Release(right);
        return nullptr;
    }

    return ret;
}

Node *Parser::ParseOOBCall() {
    Pos start = this->tkcur_.start;
    TokenType kind = this->tkcur_.type;
    Node *call;

    this->Eat();

    if ((call = this->ParseExpr()) == nullptr)
        return nullptr;

    if (!AR_TYPEOF(call, type_ast_call_)) {
        Release(call);
        return (Node *) ErrorFormat(type_syntax_error_, "%s expect call expression",
                                    kind == TokenType::DEFER ? "defer" : "spawn");
    }

    call->start = start;
    call->kind = kind;

    return call;
}

Node *Parser::ParseBlock(bool nostatic) {
    Node *tmp;
    List *stmt;
    Pos start;
    Pos end;

    start = this->tkcur_.start;

    if (!this->MatchEat(TokenType::LEFT_BRACES, false))
        return (Node *) ErrorFormat(type_syntax_error_, "expected '{'");

    if ((stmt = ListNew()) == nullptr)
        return nullptr;

    end = this->tkcur_.end;
    while (!this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        if ((tmp = this->ParseDecls(nostatic)) == nullptr) {
            Release(stmt);
            return nullptr;
        }

        if (!ListAppend(stmt, tmp)) {
            Release(tmp);
            Release(stmt);
            return nullptr;
        }

        Release(tmp);
        end = this->tkcur_.end;
    }

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_block_)) == nullptr) {
        Release(stmt);
        return nullptr;
    }

    tmp->start = start;
    tmp->end = end;
    tmp->colno = 0;
    tmp->lineno = 0;

    ((Unary *) tmp)->value = stmt;

    return tmp;
}

Node *Parser::TypeDeclBlock(bool is_struct) {
    List *stmt;
    Node *tmp;
    Pos start;
    Pos end;
    bool pub;

    start = this->tkcur_.start;

    if (!this->MatchEat(TokenType::LEFT_BRACES, false))
        return (Node *) ErrorFormat(type_syntax_error_, "expected '{'");

    if ((stmt = ListNew()) == nullptr)
        return nullptr;

    end = this->tkcur_.end;
    while (!this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        pub = false;

        if (this->MatchEat(TokenType::PUB, true))
            pub = true;

        switch (this->tkcur_.type) {
            case TokenType::LET:
                tmp = this->ParseVarDecl(true, pub);
                break;
            case TokenType::FUNC:
                tmp = this->FuncDecl(pub, !is_struct);
                break;
            default:
                if (is_struct) {
                    if (this->Match(TokenType::VAR, TokenType::WEAK)) {
                        tmp = this->ParseVarDecl(false, pub);
                        break;
                    }
                }

                Release(stmt);
                return (Node *) ErrorFormat(type_syntax_error_, is_struct ? "expected var/let or func declaration"
                                                                          : "expected let or func declaration");
        }

        if (tmp == nullptr || !ListAppend(stmt, tmp)) {
            Release(tmp);
            Release(stmt);
            return nullptr;
        }

        Release(tmp);
        end = this->tkcur_.end;
    }

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_block_)) == nullptr) {
        Release(stmt);
        return nullptr;
    }

    tmp->start = start;
    tmp->end = end;
    tmp->colno = 0;
    tmp->lineno = 0;

    ((Unary *) tmp)->value = stmt;

    return tmp;
}

Node *Parser::ParseScope() {
    Node *ltmp;
    ArObject *rtmp;
    Binary *scope;

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier");

    if ((ltmp = this->ParseIdentifier()) == nullptr)
        return nullptr;

    while (this->Match(TokenType::SCOPE)) {
        this->Eat();

        if (!this->Match(TokenType::IDENTIFIER)) {
            Release(ltmp);
            return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after '::' access operator");
        }

        if ((rtmp = StringNew((const char *) this->tkcur_.buf)) == nullptr) {
            Release(ltmp);
            return nullptr;
        }

        if ((scope = ArObjectNew<Binary>(RCType::INLINE, type_ast_selector_)) == nullptr) {
            Release(ltmp);
            Release(rtmp);
            return nullptr;
        }

        scope->kind = TokenType::SCOPE;
        scope->start = ltmp->start;
        scope->end = this->tkcur_.end;
        scope->colno = 0;
        scope->lineno = 0;
        scope->left = ltmp;
        scope->right = rtmp;

        this->Eat();

        ltmp = scope;
        rtmp = nullptr;
        scope = nullptr;
    }

    return ltmp;
}

Node *Parser::Expression() {
    Node *left;
    Node *ret;

    if ((left = this->ParseExpr()) == nullptr)
        return nullptr;

    if (this->Match(TokenType::COLON))
        return left; // Return identifier, this is a label!

    // This trick allows us to check if there is an assignment expression under the Null Safety expression.
    ret = left;
    if (AR_TYPEOF(ret, type_ast_safe_))
        ret = (Node *) ((Unary *) ret)->value;

    if (!AR_TYPEOF(ret, type_ast_assignment_)) {
        if ((ret = ArObjectNew<Unary>(RCType::INLINE, type_ast_expression_)) == nullptr) {
            Release(left);
            return nullptr;
        }

        ret->start = left->start;
        ret->end = left->end;
        ret->colno = 0;
        ret->lineno = 0;
        ((Unary *) ret)->value = left;

        return ret;
    }

    return left;
}

Node *Parser::ParseMultiDecl(const Token &token) {
    Assignment *multi;
    Node *id;
    List *ids;
    Pos end;

    if ((id = MakeIdentifier(token)) == nullptr)
        return nullptr;

    if ((ids = ListNew()) == nullptr) {
        Release(id);
        return nullptr;
    }

    if (!ListAppend(ids, id)) {
        Release(id);
        Release(ids);
        return nullptr;
    }

    do {
        if ((id = this->ParseIdentifier()) == nullptr) {
            Release(ids);
            return nullptr;
        }

        if (!ListAppend(ids, id)) {
            Release(id);
            Release(ids);
            return nullptr;
        }

        Release(id);

        end = this->tkcur_.end;
    } while (this->MatchEat(TokenType::COMMA, true));

    if ((multi = ArObjectNew<Assignment>(RCType::INLINE, type_ast_list_decl_)) == nullptr) {
        Release(ids);
        return nullptr;
    }

    multi->start = token.start;
    multi->end = end;
    multi->colno = 0;
    multi->lineno = 0;
    multi->name = ids;
    multi->value = nullptr;

    return multi;
}

Node *Parser::ParseVarDecl(bool constant, bool pub) {
    Assignment *assign = nullptr;
    Node *tmp = nullptr;
    bool weak = false;
    bool multi = false;
    Token tmp_tok;
    Pos start;

    if (!constant && this->MatchEat(TokenType::WEAK, false))
        weak = true;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER)) {
        ErrorFormat(type_syntax_error_, "expected identifier after %s keyword", constant ? "let" : "var");
        return nullptr;
    }

    tmp_tok = std::move(this->tkcur_);

    this->Eat();
    if (!this->MatchEat(TokenType::COMMA, true)) {
        if ((assign = AssignmentNew(tmp_tok, constant, pub, weak)) == nullptr)
            return nullptr;
    } else {
        if ((assign = (Assignment *) this->ParseMultiDecl(tmp_tok)) == nullptr)
            return nullptr;
        multi = true;
    }

    assign->start = start;
    assign->constant = constant;
    assign->pub = pub;
    assign->weak = weak;

    if (this->MatchEat(TokenType::EQUAL, true)) {
        if ((tmp = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr) {
            Release(assign);
            return nullptr;
        }

        assign->end = tmp->end;
        assign->value = tmp;

        if (multi && AR_TYPEOF(tmp, type_ast_tuple_)) {
            const auto *ids = (List *) assign->name;
            const auto *values = (List *) ((Unary *) tmp)->value;

            if (values->len > ids->len) {
                ErrorFormat(type_syntax_error_, "values to be assigned exceeded of: %d", values->len - ids->len);
                Release(assign);
                return nullptr;
            }
        }
    } else if (constant) {
        ErrorFormat(type_syntax_error_, "expected = after identifier/s in let declaration");
        Release(assign);
        return nullptr;
    }

    return assign;
}

Node *Parser::ParseElvis(Node *left) {
    Pos start = this->tkcur_.start;
    Node *right;
    Test *test;

    this->Eat();

    if ((right = this->ParseExpr()) == nullptr)
        return nullptr;

    if ((test = ArObjectNew<Test>(RCType::INLINE, type_ast_elvis_)) == nullptr) {
        Release(right);
        return nullptr;
    }

    test->start = start;
    test->end = right->end;
    test->colno = 0;
    test->lineno = 0;

    test->test = left;
    test->body = nullptr;
    test->orelse = right;

    return test;
}

Node *Parser::ParseExpr(int precedence) {
    bool no_init = this->no_init_;
    bool safe = false;
    LedMeth led;
    NudMeth nud;

    Node *left;
    Node *right;

    if ((nud = this->LookupNud()) == nullptr)
        return (Node *) ErrorFormat(type_syntax_error_, "invalid token found");

    if ((left = (this->*nud)()) == nullptr)
        return nullptr;

    while (!this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON)
           && precedence < PeekPrecedence(this->tkcur_.type)) {

        if (this->tkcur_.type == TokenType::LEFT_BRACES && no_init)
            break;

        if ((led = this->LookupLed()) == nullptr)
            break;

        if ((right = (this->*led)(left)) == nullptr) {
            Release(left);
            return nullptr;
        }

        if (left->kind == TokenType::QUESTION_DOT || right->kind == TokenType::QUESTION_DOT)
            safe = true;

        left = right;
    }

    if (safe) {
        // Encapsulates "null safety" expressions, e.g.: a?.b, a.b?.c(), a?.b = c?.o
        if ((right = ArObjectNew<Unary>(RCType::INLINE, type_ast_safe_)) == nullptr) {
            Release(left);
            return nullptr;
        }

        right->start = 0;
        right->end = 0;
        right->colno = 0;
        right->lineno = 0;

        ((Unary *) right)->value = left;

        return right;
    }

    return left;
}

Node *Parser::ParseFnCall(Node *left) {
    Node *arg;
    Node *tmp;
    List *args;

    Pos end;

    bool exit = false;

    this->Eat();

    if ((args = ListNew()) == nullptr)
        return nullptr;

    this->EatTerm();
    if (!this->Match(TokenType::RIGHT_ROUND)) {
        do {
            if ((arg = this->ParseExpr(EXPR_NO_LIST)) == nullptr) {
                Release(args);
                return nullptr;
            }

            if (this->Match(TokenType::ELLIPSIS)) {
                if ((tmp = SpreadNew(arg, this->tkcur_.end)) == nullptr)
                    goto ERROR;

                arg = tmp;
                exit = true;
                this->Eat();
            }

            if (!ListAppend(args, arg))
                goto ERROR;

            Release(arg);
        } while (!exit && this->MatchEat(TokenType::COMMA, true));
    }

    end = this->tkcur_.end;

    if (!this->MatchEat(TokenType::RIGHT_ROUND, false)) {
        ErrorFormat(type_syntax_error_, "expected ')' after last argument of function call");
        Release(args);
        return nullptr;
    }

    if ((tmp = ArObjectNew<Binary>(RCType::INLINE, type_ast_call_)) == nullptr) {
        Release(args);
        return nullptr;
    }

    tmp->start = left->start;
    tmp->end = end;
    tmp->kind = TokenType::TK_NULL;
    tmp->colno = 0;
    tmp->lineno = 0;

    ((Binary *) tmp)->left = left;
    ((Binary *) tmp)->right = args;
    return tmp;

    ERROR:
    Release(arg);
    Release(args);
    return nullptr;
}

Node *Parser::ParseFromImport() {
    ArObject *imports = nullptr;
    ArObject *module;
    String *path;
    ImportDecl *ret;
    Pos start;
    Pos end;

    start = this->tkcur_.start;
    this->Eat();

    if ((module = this->ScopeAsName(false, false)) == nullptr)
        return nullptr;

    path = (String *) IncRef(((Binary *) module)->left);
    Release(module);

    if (!this->MatchEat(TokenType::IMPORT, false)) {
        Release(path);
        return nullptr;
    }

    if (!this->Match(TokenType::ASTERISK)) {
        if ((imports = this->ScopeAsNameList(true, true)) == nullptr) {
            Release(path);
            return nullptr;
        }
    } else {
        end = this->tkcur_.end;
        this->Eat();
    }

    if ((ret = ImportNew(path, imports, start)) == nullptr) {
        Release(path);
        Release(imports);
        return nullptr;
    }

    if (imports == nullptr)
        ret->end = end;

    return ret;
}

Node *Parser::ParseFor() {
    const TypeInfo *type = type_ast_for_;
    Node *init = nullptr;
    Node *test = nullptr;
    Node *inc = nullptr;
    Node *body = nullptr;
    Loop *loop;
    Pos start;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->MatchEat(TokenType::SEMICOLON, false)) {
        if (this->Match(TokenType::VAR))
            init = this->ParseVarDecl(false, false);
        else
            init = this->ParseExpr();

        if (init == nullptr)
            goto ERROR;
    }

    if (this->MatchEat(TokenType::IN, false)) {
        type = type_ast_for_in_;

        if (init == nullptr || !AR_TYPEOF(init, type_ast_identifier_)) {
            if (!AR_TYPEOF(init, type_ast_tuple_) || !IsIdentifiersList(init)) {
                Release(init);
                return (Node *) ErrorFormat(type_syntax_error_, "expected identifier or tuple before 'in'");
            }
        }
    } else {
        if (!this->MatchEat(TokenType::SEMICOLON, true)) {
            Release(init);
            return (Node *) ErrorFormat(type_syntax_error_, "expected ';' after for 'init'");
        }
    }

    if (type == type_ast_for_) {
        if ((test = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr)
            goto ERROR;

        if (!this->MatchEat(TokenType::SEMICOLON, true)) {
            ErrorFormat(type_syntax_error_, "expected ';' after for 'test'");
            goto ERROR;
        }

        this->no_init_ = true;
        if ((inc = this->Expression()) == nullptr)
            goto ERROR;
        this->no_init_ = false;
    } else {
        this->no_init_ = true;
        if ((test = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr)
            goto ERROR;
        this->no_init_ = false;
    }

    if ((body = this->ParseBlock(true)) == nullptr)
        goto ERROR;

    if ((loop = ArObjectNew<Loop>(RCType::INLINE, type)) == nullptr)
        goto ERROR;

    loop->start = start;
    loop->end = body->end;
    loop->colno = 0;
    loop->lineno = 0;

    loop->init = init;
    loop->test = test;
    loop->inc = inc;
    loop->body = body;

    return loop;

    ERROR:
    Release(init);
    Release(test);
    Release(inc);
    Release(body);
    return nullptr;
}

Node *Parser::FuncDecl(bool pub, bool nobody) {
    String *name = nullptr;
    List *params = nullptr;
    Node *tmp = nullptr;
    Construct *fn;
    Pos start;
    Pos end;
    bool exit;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after 'func' keyword");

    if ((name = StringNew((const char *) this->tkcur_.buf)) == nullptr)
        return nullptr;

    end = this->tkcur_.end;

    this->Eat();

    if (this->MatchEat(TokenType::LEFT_ROUND, false)) {
        if ((params = ListNew()) == nullptr)
            goto ERROR;

        exit = false;

        do {
            if (this->Match(TokenType::RIGHT_ROUND))
                break;

            if (this->ParseRestElement(&tmp))
                exit = true;
            else
                tmp = this->ParseIdentifier();

            if (tmp == nullptr || !ListAppend(params, tmp))
                goto ERROR;

            Release(tmp);
            tmp = nullptr;
        } while (!exit && this->MatchEat(TokenType::COMMA, true));

        end = this->tkcur_.end;

        if (!this->MatchEat(TokenType::RIGHT_ROUND, true)) {
            ErrorFormat(type_syntax_error_, "expected ')' after function params");
            goto ERROR;
        }
    }

    tmp = nullptr;
    if (!nobody || this->Match(TokenType::LEFT_BRACES)) {
        tmp = this->ParseBlock(true);
        if (tmp == nullptr)
            goto ERROR;

        end = tmp->end;
    }

    if ((fn = FunctionNew(start, end, name, params, tmp, pub)) == nullptr)
        goto ERROR;

    return fn;

    ERROR:
    Release(name);
    Release(params);
    Release(tmp);
    return nullptr;
}

Node *Parser::ParseIdentifier() {
    Unary *id;

    if (!this->Match(TokenType::IDENTIFIER, TokenType::BLANK, TokenType::SELF))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier");

    id = (Unary *) MakeIdentifier(this->tkcur_);

    this->Eat();

    return id;
}

Node *Parser::ParseJmpStmt() {
    Pos start = this->tkcur_.start;
    Pos end = this->tkcur_.end;
    TokenType kind = this->tkcur_.type;
    String *id = nullptr;
    Node *label = nullptr;
    Unary *ret;

    this->Eat();

    if (this->Match(TokenType::IDENTIFIER)) {
        if ((label = this->ParseIdentifier()) == nullptr)
            return nullptr;
        end = label->end;

        id = (String *) IncRef(((Unary *) label)->value);
        Release(label);
    }

    if ((ret = ArObjectNew<Unary>(RCType::INLINE, type_ast_jmp_)) == nullptr) {
        Release(label);
        return nullptr;
    }

    ret->kind = kind;
    ret->start = start;
    ret->end = end;

    ret->colno = 0;
    ret->lineno = 0;

    ret->value = id;
    return ret;
}

Node *Parser::ParseList() {
    Pos start = this->tkcur_.start;
    List *list;
    Node *tmp;

    this->Eat();

    if ((list = ListNew()) == nullptr)
        return nullptr;

    this->EatTerm();
    if (!this->Match(TokenType::RIGHT_SQUARE)) {
        do {
            if ((tmp = this->ParseExpr(EXPR_NO_LIST)) == nullptr)
                goto ERROR;

            if (!ListAppend(list, tmp))
                goto ERROR;

            Release(tmp);
        } while (this->MatchEat(TokenType::COMMA, true));
    }

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_list_)) == nullptr)
        goto ERROR;

    tmp->start = start;
    tmp->end = this->tkcur_.end;
    tmp->colno = 0;
    tmp->lineno = 0;
    ((Unary *) tmp)->value = list;

    this->EatTerm();

    if (!this->MatchEat(TokenType::RIGHT_SQUARE, false)) {
        Release(tmp);
        return (Node *) ErrorFormat(type_syntax_error_, "expected ']' after list definition");
    }

    return tmp;

    ERROR:
    Release(list);
    Release(tmp);
    return nullptr;
}

Node *Parser::ParseLiteral() {
    Unary *literal;
    ArObject *value;

    // TODO: optimize with HoldBuffer
    switch (this->tkcur_.type) {
        case TokenType::ATOM:
            value = AtomNew(this->tkcur_.buf);
            break;
        case TokenType::NUMBER:
            value = IntegerNew((const char *) this->tkcur_.buf, 10);
            break;
        case TokenType::NUMBER_BIN:
            value = IntegerNew((const char *) this->tkcur_.buf, 2);
            break;
        case TokenType::NUMBER_OCT:
            value = IntegerNew((const char *) this->tkcur_.buf, 8);
            break;
        case TokenType::NUMBER_HEX:
            value = IntegerNew((const char *) this->tkcur_.buf, 16);
            break;
        case TokenType::DECIMAL:
            value = DecimalNew((const char *) this->tkcur_.buf);
            break;
        case TokenType::NUMBER_CHR:
            value = IntegerNew(StringUTF8toInt((const unsigned char *) this->tkcur_.buf));
            break;
        case TokenType::STRING:
        case TokenType::RAW_STRING:
            value = StringNew((const char *) this->tkcur_.buf, this->tkcur_.buflen);
            break;
        case TokenType::BYTE_STRING:
            value = BytesNew(this->tkcur_.buf, this->tkcur_.buflen, true);
            break;
        case TokenType::FALSE:
            value = IncRef(False);
            break;
        case TokenType::TRUE:
            value = IncRef(True);
            break;
        case TokenType::NIL:
            value = IncRef(NilVal);
            break;
        default:
            assert(false); // Never get here!
    }

    if (value == nullptr)
        return nullptr;

    if ((literal = ArObjectNew<Unary>(RCType::INLINE, type_ast_literal_)) == nullptr) {
        Release(value);
        return nullptr;
    }

    literal->start = this->tkcur_.start;
    literal->end = this->tkcur_.end;
    literal->colno = 0;
    literal->lineno = 0;
    literal->value = value;

    this->Eat();

    return literal;
}

Node *Parser::ParseLoop() {
    Loop *loop;

    if ((loop = ArObjectNew<Loop>(RCType::INLINE, type_ast_loop_)) == nullptr)
        return nullptr;

    loop->start = this->tkcur_.start;
    loop->colno = 0;
    loop->lineno = 0;

    loop->init = nullptr;
    loop->test = nullptr;
    loop->inc = nullptr;

    this->Eat();

    if (this->Match(TokenType::LEFT_BRACES)) {
        if ((loop->body = this->ParseBlock(true)) == nullptr) {
            Release(loop);
            return nullptr;
        }
    } else {
        this->no_init_ = true;
        if ((loop->test = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr) {
            Release(loop);
            return nullptr;
        }
        this->no_init_ = false;

        if ((loop->body = this->ParseBlock(true)) == nullptr) {
            Release(loop);
            return nullptr;
        }
    }

    loop->end = loop->body->end;

    return loop;
}

Node *Parser::ParseIf() {
    Test *test;

    if ((test = ArObjectNew<Test>(RCType::INLINE, type_ast_test_)) == nullptr)
        return nullptr;

    test->start = this->tkcur_.start;
    test->body = nullptr;
    test->orelse = nullptr;

    this->Eat();

    this->no_init_ = true;
    if ((test->test = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr)
        goto ERROR;
    this->no_init_ = false;

    if ((test->body = this->ParseBlock(false)) == nullptr)
        goto ERROR;

    test->end = ((Node *) test->body)->end;

    if (this->Match(TokenType::ELIF)) {
        if ((test->orelse = this->ParseIf()) == nullptr)
            goto ERROR;

        test->end = ((Node *) test->orelse)->end;
    } else if (this->MatchEat(TokenType::ELSE, false)) {
        if ((test->orelse = this->ParseBlock(false)) == nullptr)
            goto ERROR;

        test->end = ((Node *) test->orelse)->end;
    }

    return test;

    ERROR:
    Release(test);
    return nullptr;
}

Node *Parser::ParseInfix(Node *left) {
    TokenType kind = this->tkcur_.type;
    Node *right;
    Binary *binary;

    this->Eat();

    if ((right = this->ParseExpr(PeekPrecedence(kind))) == nullptr)
        return nullptr;

    if ((binary = BinaryNew(kind, type_ast_binary_, left, right)) == nullptr)
        Release(right);

    return binary;
}

Node *Parser::ParseInitialization(Node *left) {
    ArObject *list;
    Binary *init;
    Pos end;
    bool is_map;

    this->Eat();

    list = this->ParseArgsKwargs("expected identifier as key",
                                 "missing value after key in struct initialization",
                                 &is_map);

    if (list == nullptr)
        return nullptr;

    end = this->tkcur_.end;

    if (!this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        Release(list);
        return (Node *) ErrorFormat(type_syntax_error_, "expected '}' after struct initialization");
    }

    if ((init = InitNew(left, list, end, is_map)) == nullptr)
        Release(list);

    return init;
}

Node *Parser::ParseImport() {
    Pos start = this->tkcur_.start;
    ArObject *imports;
    ImportDecl *ret;

    this->Eat();

    if ((imports = this->ScopeAsNameList(false, true)) == nullptr)
        return nullptr;

    if ((ret = ImportNew(nullptr, imports, start)) == nullptr) {
        Release(imports);
        return nullptr;
    }

    return ret;
}

Node *Parser::ParseMapSet() {
    Pos start = this->tkcur_.start;
    ArObject *list;
    Unary *tmp;
    bool is_map;

    this->Eat();

    list = this->ParseArgsKwargs("you started defining a set, not a map",
                                 "missing value after key in map definition",
                                 &is_map);

    if (list == nullptr)
        return nullptr;

    // An empty {}, is considered as empty map!
    if (!is_map && ((List *) list)->len == 0)
        is_map = true;

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, is_map ? type_ast_map_ : type_ast_set_)) == nullptr) {
        Release(list);
        return nullptr;
    }

    tmp->start = start;
    tmp->end = this->tkcur_.end;
    tmp->colno = 0;
    tmp->lineno = 0;
    tmp->value = list;

    if (!this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        Release(tmp);
        return (Node *) ErrorFormat(type_syntax_error_, "expected '}' after %s definition", is_map ? "map" : "set");
    }

    return tmp;
}

Node *Parser::ParseMemberAccess(Node *left) {
    TokenType kind = this->tkcur_.type;
    String *id = nullptr;
    Binary *binary;
    Node *right;
    Pos end;

    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after '.'/'?.'/'::' access operator");

    if ((right = this->ParseExpr(PeekPrecedence(kind))) == nullptr)
        return nullptr;

    end = right->end;

    if (AR_TYPEOF(right, type_ast_identifier_)) {
        id = (String *) IncRef(((Unary *) right)->value);
        Release((ArObject **) &right);
    }

    if ((binary = ArObjectNew<Binary>(RCType::INLINE, type_ast_selector_)) != nullptr) {
        binary->kind = kind;
        binary->start = left->start;
        binary->end = end;
        binary->colno = 0;
        binary->lineno = 0;
        binary->left = left;
        binary->right = id != nullptr ? (ArObject *) id : IncRef(right);
    }

    Release(right);

    return binary;
}

Node *Parser::ParsePanic() {
    Pos start = this->tkcur_.start;
    Node *expr;
    Unary *unary;

    this->Eat();

    if ((expr = this->ParseExpr()) == nullptr)
        return nullptr;

    if ((unary = ArObjectNew<Unary>(RCType::INLINE, type_ast_panic_)) == nullptr) {
        Release(expr);
        return nullptr;
    }

    unary->kind = TokenType::PANIC;
    unary->start = start;
    unary->end = expr->end;

    unary->value = expr;

    return unary;
}

Node *Parser::ParsePipeline(Node *left) {
    Node *right;
    Binary *call;
    List *args;

    this->Eat();

    //                                          ▼▼▼▼▼ behave like a function call ▼▼▼▼▼
    if ((right = this->ParseExpr(PeekPrecedence(TokenType::LEFT_ROUND) - 1)) == nullptr)
        return nullptr;

    if (AR_TYPEOF(right, type_ast_call_)) {
        call = ((Binary *) right);
        args = (List *) call->right;

        if (!ListInsertAt(args, left, 0)) {
            Release(right);
            return nullptr;
        }
    } else {
        if ((args = ListNew()) == nullptr) {
            Release(right);
            return nullptr;
        }

        ListAppend(args, left);

        if ((call = ArObjectNew<Binary>(RCType::INLINE, type_ast_call_)) == nullptr) {
            Release(right);
            Release(args);
            return nullptr;
        }

        call->left = right;
        call->right = args;
    }

    call->start = left->start;
    call->end = right->end;
    call->kind = TokenType::TK_NULL;
    call->colno = 0;
    call->lineno = 0;
    return call;
}

Node *Parser::ParsePostUpdate(Node *left) {
    UpdateIncDec *update;

    if (!AR_TYPEOF(left, type_ast_identifier_)
        && !AR_TYPEOF(left, type_ast_index_)
        && !AR_TYPEOF(left, type_ast_selector_))
        return (Node *) ErrorFormat(type_syntax_error_, "unexpected update operator");

    update = UpdateNew(this->tkcur_.type, this->tkcur_.end, false, left);

    this->Eat();

    return update;
}

Node *Parser::ParsePreUpdate() {
    TokenType kind = this->tkcur_.type;
    Pos start = this->tkcur_.start;
    UpdateIncDec *update;
    Node *right;

    this->Eat();

    if ((right = this->ParseExpr(0)) == nullptr)
        return nullptr;

    if (!AR_TYPEOF(right, type_ast_identifier_)
        && !AR_TYPEOF(right, type_ast_index_)
        && !AR_TYPEOF(right, type_ast_selector_)) {
        Release(right);
        return (Node *) ErrorFormat(type_syntax_error_, "unexpected update operator");
    }

    if ((update = UpdateNew(this->tkcur_.type, this->tkcur_.end, true, right)) == nullptr)
        Release(right);

    return update;
}

Node *Parser::ParsePrefix() {
    TokenType kind = this->tkcur_.type;
    Pos start = this->tkcur_.start;
    Node *right;
    Unary *unary;

    this->Eat();

    if ((right = this->ParseExpr(PeekPrecedence(kind))) == nullptr)
        return nullptr;

    if ((unary = UnaryNew(kind, start, right)) == nullptr)
        Release(right);

    return unary;
}

Node *Parser::ParseReturn() {
    Pos start = this->tkcur_.start;
    Pos end;
    Unary *tmp;

    this->Eat();

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_ret_)) == nullptr)
        return nullptr;

    tmp->start = start;
    tmp->end = this->tkcur_.end;
    tmp->colno = 0;
    tmp->lineno = 0;

    tmp->value = nullptr;
    if (!this->Match(TokenType::END_OF_LINE, TokenType::END_OF_FILE, TokenType::SEMICOLON)) {
        if ((tmp->value = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr) {
            Release(tmp);
            return nullptr;
        }

        tmp->end = ((Node *) tmp->value)->end;
    }

    return tmp;
}

Node *Parser::ParseYield() {
    Pos start = this->tkcur_.start;
    Pos end;
    Unary *tmp;

    this->Eat();

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_yield_)) == nullptr)
        return nullptr;

    tmp->start = start;
    tmp->end = this->tkcur_.end;
    tmp->colno = 0;
    tmp->lineno = 0;

    tmp->value = nullptr;

    if (this->Match(TokenType::END_OF_LINE, TokenType::END_OF_FILE, TokenType::SEMICOLON)) {
        Release(tmp);
        return (Node *) ErrorFormat(type_syntax_error_, "expected expression after yield");
    }

    if ((tmp->value = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr) {
        Release(tmp);
        return nullptr;
    }

    tmp->end = ((Node *) tmp->value)->end;

    return tmp;
}

Node *Parser::ParseStatement() {
    Pos start = this->tkcur_.start;
    Node *label = nullptr;
    Node *tmp;
    Binary *ret;

    do {
        if (this->TokenInRange(TokenType::KEYWORD_BEGIN, TokenType::KEYWORD_END)) {
            switch (this->tkcur_.type) {
                case TokenType::ASSERT:
                    tmp = this->ParseAssertion();
                    break;
                case TokenType::DEFER:
                case TokenType::SPAWN:
                    tmp = this->ParseOOBCall();
                    break;
                case TokenType::RETURN:
                    tmp = this->ParseReturn();
                    break;
                case TokenType::YIELD:
                    tmp = this->ParseYield();
                    break;
                case TokenType::IMPORT:
                    tmp = this->ParseImport();
                    break;
                case TokenType::FROM:
                    tmp = this->ParseFromImport();
                    break;
                case TokenType::FOR:
                    tmp = this->ParseFor();
                    break;
                case TokenType::LOOP:
                    tmp = this->ParseLoop();
                    break;
                case TokenType::PANIC:
                    tmp = this->ParsePanic();
                    break;
                case TokenType::IF:
                    tmp = this->ParseIf();
                    break;
                case TokenType::SWITCH:
                    tmp = this->SwitchDecl();
                    break;
                case TokenType::BREAK:
                case TokenType::CONTINUE:
                case TokenType::FALLTHROUGH:
                    tmp = this->ParseJmpStmt();
                    break;
                default:
                    return (Node *) ErrorFormat(type_syntax_error_, "unexpected token '%s'", this->tkcur_.buf);
            }
        } else
            tmp = this->Expression();

        if (tmp == nullptr) {
            Release(label);
            return nullptr;
        }

        if (!AR_TYPEOF(tmp, type_ast_identifier_) || !this->MatchEat(TokenType::COLON, false))
            break;

        // Label stmt
        this->EatTerm();

        if (label != nullptr) {
            Release(tmp);
            Release(label);
            return (Node *) ErrorFormat(type_syntax_error_, "expected statement after label");
        }
        label = tmp;
    } while (true);

    if (label != nullptr) {
        if ((ret = ArObjectNew<Binary>(RCType::INLINE, type_ast_label_)) == nullptr) {
            Release(tmp);
            Release(label);
            return nullptr;
        }

        ret->start = start;
        ret->end = tmp->end;
        ret->colno = 0;
        ret->lineno = 0;

        ret->left = IncRef(((Unary *) label)->value);
        ret->right = tmp;

        Release(label);

        return ret;
    }

    return tmp;
}

Node *Parser::SwitchCase() {
    int last_fallthrough = -1;
    List *conditions = nullptr;
    List *body = nullptr;
    Node *tmp = nullptr;
    Binary *ret;

    Pos start = this->tkcur_.start;
    Pos end = this->tkcur_.end;

    if (this->MatchEat(TokenType::CASE, false)) {
        if ((conditions = ListNew()) == nullptr)
            return nullptr;

        do {
            if ((tmp = this->ParseExpr()) == nullptr)
                goto ERROR;

            if (!ListAppend(conditions, tmp)) {
                Release(tmp);
                goto ERROR;
            }

            end = tmp->end;
            Release(tmp);
        } while (this->MatchEat(TokenType::SEMICOLON, true));
    } else if (!this->MatchEat(TokenType::DEFAULT, false))
        return (Node *) ErrorFormat(type_syntax_error_, "expected 'case' or 'default' label");

    if (!this->MatchEat(TokenType::COLON, true)) {
        Release(conditions);
        return (Node *) ErrorFormat(type_syntax_error_, "expected ':' after %s label",
                                    conditions == nullptr ? "default" : "case");
    }

    while (!this->Match(TokenType::CASE, TokenType::DEFAULT, TokenType::RIGHT_BRACES)) {
        if (body == nullptr && (body = ListNew()) == nullptr)
            goto ERROR;

        if ((tmp = this->ParseDecls(true)) == nullptr)
            goto ERROR;

        if (!ListAppend(body, tmp)) {
            Release(tmp);
            goto ERROR;
        }

        if (AR_TYPEOF(tmp, type_ast_jmp_) && tmp->kind == TokenType::FALLTHROUGH)
            last_fallthrough = (int) body->len;

        end = tmp->end;
        Release(tmp);

        this->EatTerm();
    }

    // Check fallthrough
    if (last_fallthrough != -1 && last_fallthrough != body->len) {
        Release(conditions);
        Release(body);
        return (Node *) ErrorFormat(type_syntax_error_, "fallthrough statement out of place");
    }

    if ((ret = ArObjectNew<Binary>(RCType::INLINE, type_ast_switch_case_)) == nullptr)
        goto ERROR;

    ret->start = start;
    ret->end = end;
    ret->colno = 0;
    ret->lineno = 0;

    ret->left = conditions;
    ret->right = body;

    return ret;

    ERROR:
    Release(conditions);
    Release(body);
    return nullptr;
}

Node *Parser::SwitchDecl() {
    List *cases = nullptr;
    Node *test = nullptr;
    Node *tmp;
    Test *ret;
    Pos start;
    Pos end;

    bool def = false;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->Match(TokenType::LEFT_BRACES)) {
        this->no_init_ = true;
        if ((test = this->ParseExpr(EXPR_NO_ASSIGN)) == nullptr)
            return nullptr;
        this->no_init_ = false;
    }

    if (!this->MatchEat(TokenType::LEFT_BRACES, false)) {
        Release(test);
        return (Node *) ErrorFormat(type_syntax_error_, "expected '{' after switch declaration");
    }

    this->EatTerm();

    if ((cases = ListNew()) == nullptr) {
        Release(test);
        return nullptr;
    }

    while (this->Match(TokenType::CASE, TokenType::DEFAULT)) {
        if (this->Match(TokenType::DEFAULT)) {
            if (def) {
                ErrorFormat(type_syntax_error_, "default case already defined");
                goto ERROR;
            }
            def = true;
        }

        if ((tmp = this->SwitchCase()) == nullptr)
            goto ERROR;

        if (!ListAppend(cases, tmp)) {
            Release(tmp);
            goto ERROR;
        }

        Release(tmp);
    }

    end = this->tkcur_.end;

    if (!this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        ErrorFormat(type_syntax_error_, "expected '}' after switch declaration");
        goto ERROR;
    }

    if ((ret = ArObjectNew<Test>(RCType::INLINE, type_ast_switch_)) == nullptr)
        goto ERROR;

    ret->start = start;
    ret->end = end;
    ret->colno = 0;
    ret->lineno = 0;

    ret->test = test;
    ret->body = cases;
    ret->orelse = nullptr;

    return ret;

    ERROR:
    Release(test);
    Release(cases);
    return nullptr;
}

Node *Parser::StructDecl(bool pub) {
    Pos start = this->tkcur_.start;
    Construct *construct;

    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after struct keyword");

    if ((construct = ArObjectNew<Construct>(RCType::INLINE, type_ast_struct_)) == nullptr)
        return nullptr;

    if ((construct->name = StringNew((const char *) this->tkcur_.buf)) == nullptr) {
        Release(construct);
        return nullptr;
    }

    this->Eat();

    construct->params = nullptr;
    if (this->MatchEat(TokenType::IMPL, true)) {
        if ((construct->params = (List *) this->TraitList()) == nullptr) {
            Release(construct);
            return nullptr;
        }
    }

    if ((construct->block = this->TypeDeclBlock(true)) == nullptr) {
        Release(construct);
        return nullptr;
    }

    construct->start = start;
    construct->end = construct->block->end;
    construct->colno = 0;
    construct->lineno = 0;
    construct->pub = pub;

    return construct;
}

Node *Parser::ParseSubscript(Node *left) {
    Pos start = this->tkcur_.start;
    Node *low = nullptr;
    Node *high = nullptr;
    Node *step = nullptr;
    Subscript *res;

    bool slice = false;

    this->Eat();

    if (this->MatchEat(TokenType::SCOPE, true)) {
        slice = true;
        if (!this->Match(TokenType::RIGHT_SQUARE)) {
            if ((step = this->ParseExpr()) == nullptr)
                goto ERROR;
        }
    } else {
        if (!this->Match(TokenType::COLON)) {
            if ((low = this->ParseExpr()) == nullptr)
                goto ERROR;
        }

        if (!this->MatchEat(TokenType::SCOPE, true)) {
            if (this->MatchEat(TokenType::COLON, true)) {
                slice = true;
                if (!this->Match(TokenType::RIGHT_SQUARE)) {
                    if ((high = this->ParseExpr()) == nullptr)
                        goto ERROR;

                    if (this->MatchEat(TokenType::COLON, false)) {
                        if ((step = this->ParseExpr()) == nullptr)
                            goto ERROR;
                    }
                }
            }
        } else {
            slice = true;
            if (!this->Match(TokenType::RIGHT_SQUARE)) {
                if ((step = this->ParseExpr()) == nullptr)
                    goto ERROR;
            }
        }
    }

    if ((res = SubscriptNew(left, slice)) == nullptr)
        goto ERROR;

    res->start = start;
    res->end = this->tkcur_.end;
    res->colno = 0;
    res->lineno = 0;

    res->low = low;
    res->high = high;
    res->step = step;

    if (!this->MatchEat(TokenType::RIGHT_SQUARE, true)) {
        Release(res);
        return (Node *) ErrorFormat(type_syntax_error_, "expected ']' after %s definition", slice ? "slice" : "index");
    }

    return res;

    ERROR:
    Release(low);
    Release(high);
    Release(step);
    return nullptr;
}

Node *Parser::ParseTernary(Node *left) {
    Pos start = this->tkcur_.start;
    Node *orelse = nullptr;
    Node *body;
    Test *test;

    this->Eat();

    if ((body = this->ParseExpr()) == nullptr)
        return nullptr;

    if (this->MatchEat(TokenType::COLON, false)) {
        orelse = this->ParseExpr();
        if (orelse == nullptr) {
            Release(body);
            return nullptr;
        }
    }

    if ((test = ArObjectNew<Test>(RCType::INLINE, type_ast_ternary_)) == nullptr) {
        Release(body);
        Release(orelse);
        return nullptr;
    }

    test->start = start;
    test->end = orelse == nullptr ? body->end : orelse->end;
    test->colno = 0;
    test->lineno = 0;

    test->test = left;
    test->body = body;
    test->orelse = orelse;

    return test;
}

Node *Parser::ParseExprList(Node *left) {
    int precedence = PeekPrecedence(this->tkcur_.type);
    Pos end;
    Unary *ret;
    Node *tmp;
    List *list;

    if ((list = ListNew()) == nullptr)
        return nullptr;

    if (!ListAppend(list, left)) {
        Release(list);
        return nullptr;
    }

    this->Eat();

    do {
        if ((tmp = this->ParseExpr(precedence)) == nullptr) {
            Release(list);
            return nullptr;
        }

        if (!ListAppend(list, tmp)) {
            Release(list);
            Release(tmp);
            return nullptr;
        }

        end = tmp->end;
        Release(tmp);
    } while (this->MatchEat(TokenType::COMMA, true));

    if ((ret = ArObjectNew<Unary>(RCType::INLINE, type_ast_tuple_)) == nullptr) {
        Release(list);
        return nullptr;
    }

    Release(left); // Already in List!

    ret->start = left->start;
    ret->end = end;
    ret->colno = 0;
    ret->lineno = 0;
    ((Unary *) ret)->value = list;

    return ret;
}

Node *Parser::TraitDecl(bool pub) {
    Pos start = this->tkcur_.start;
    Construct *construct;

    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after trait keyword");

    if ((construct = ArObjectNew<Construct>(RCType::INLINE, type_ast_trait_)) == nullptr)
        return nullptr;

    if ((construct->name = StringNew((const char *) this->tkcur_.buf)) == nullptr) {
        Release(construct);
        return nullptr;
    }

    this->Eat();

    construct->params = nullptr;
    if (this->MatchEat(TokenType::COLON, true)) {
        if ((construct->params = (List *) this->TraitList()) == nullptr) {
            Release(construct);
            return nullptr;
        }
    }

    if ((construct->block = this->TypeDeclBlock(false)) == nullptr) {
        Release(construct);
        return nullptr;
    }

    construct->start = start;
    construct->end = construct->block->end;
    construct->colno = 0;
    construct->lineno = 0;
    construct->pub = pub;

    return construct;
}

Node *Parser::ParseTupleLambda() {
    Pos start = this->tkcur_.start;
    Pos end;
    bool must_fn;
    bool last_is_comma;

    List *args;
    Node *tmp;
    Construct *fn;

    this->Eat();

    if ((args = ListNew()) == nullptr)
        return nullptr;

    must_fn = false;
    last_is_comma = false;

    this->EatTerm();
    if (!this->Match(TokenType::RIGHT_ROUND)) {
        must_fn = true;
        if (this->ParseRestElement(&tmp)) {
            if (tmp == nullptr || !ListAppend(args, tmp))
                goto ERROR;

            Release(tmp);
        } else {
            must_fn = false;
            do {
                if (this->Match(TokenType::RIGHT_ROUND))
                    break;

                last_is_comma = false;

                if (this->ParseRestElement(&tmp))
                    must_fn = true;
                else
                    tmp = this->ParseExpr(EXPR_NO_LIST);

                if (tmp == nullptr || !ListAppend(args, tmp))
                    goto ERROR;

                Release(tmp);

                if (!this->MatchEat(TokenType::COMMA, true))
                    break;

                last_is_comma = true;
            } while (true);
        }
    }

    end = this->tkcur_.end;
    if (!this->MatchEat(TokenType::RIGHT_ROUND, true)) {
        Release(args);
        return (Node *) ErrorFormat(type_syntax_error_, "expected ')'");
    }

    if (this->MatchEat(TokenType::ARROW, false)) {
        // All items into params list must be IDENTIFIER
        for (int i = 0; i < args->len; i++) {
            auto *obj = args->objects[i];
            if (!AR_TYPEOF(obj, type_ast_identifier_) && !AR_TYPEOF(obj, type_ast_restid_)) {
                Release(args);
                return (Node *) ErrorFormat(type_syntax_error_,
                                            "expected identifier as %d element in arrow definition", i);
            }
        }

        if ((tmp = this->ParseBlock(true)) == nullptr) {
            Release(args);
            return nullptr;
        }

        if ((fn = FunctionNew(start, tmp->end, nullptr, args, tmp, false)) == nullptr)
            goto ERROR;

        return fn;
    }

    if (must_fn) {
        Release(args);
        return (Node *) ErrorFormat(type_syntax_error_, "expected arrow function declaration");
    }

    // parenthesized expression
    if (!last_is_comma && args->len == 1) {
        tmp = (Node *) ListGetItem(args, 0);
        Release(args);
        return tmp;
    }

    // it's definitely a tuple
    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_tuple_)) == nullptr) {
        Release(args);
        return nullptr;
    }

    tmp->start = start;
    tmp->end = end;
    tmp->colno = 0;
    tmp->lineno = 0;
    ((Unary *) tmp)->value = args;

    return tmp;

    ERROR:
    Release(tmp);
    Release(args);
    return nullptr;
}

Node *Parser::ScopeAsName(bool id_only, bool with_alias) {
    String *id = nullptr;
    String *scope_sep = nullptr;
    Binary *ret;
    String *paths;
    String *tmp;

    Pos start = this->tkcur_.start;
    Pos end = this->tkcur_.end;

    if (!this->Match(TokenType::IDENTIFIER)) {
        if (id_only)
            return (Node *) ErrorFormat(type_syntax_error_, "expected name");
        return (Node *) ErrorFormat(type_syntax_error_, "expected name or scope path");
    }

    if ((paths = StringNew((const char *) this->tkcur_.buf)) == nullptr)
        return nullptr;

    this->Eat();

    if (this->MatchEat(TokenType::SCOPE, false)) {
        if (id_only) {
            Release(paths);
            return (Node *) ErrorFormat(type_syntax_error_, "expected name not scope path");
        }

        if ((scope_sep = StringIntern("::")) == nullptr) {
            Release(paths);
            return nullptr;
        }

        do {
            if ((tmp = StringConcat(paths, scope_sep)) == nullptr)
                goto ERROR;

            Release(paths);

            paths = tmp;

            if (!this->Match(TokenType::IDENTIFIER)) {
                ErrorFormat(type_syntax_error_, "expected name after scope separator");
                goto ERROR;
            }

            Release(id);

            if ((id = StringNew((const char *) this->tkcur_.buf)) == nullptr)
                goto ERROR;

            end = this->tkcur_.end;
            this->Eat();

            if ((tmp = StringConcat(paths, id)) == nullptr)
                goto ERROR;

            Release(paths);

            paths = tmp;

        } while (this->MatchEat(TokenType::SCOPE, false));

        Release(scope_sep);
    } else
        id = IncRef(paths);

    if (with_alias) {
        if (this->MatchEat(TokenType::AS, false)) {
            if (!this->Match(TokenType::IDENTIFIER)) {
                ErrorFormat(type_syntax_error_, "expected alias name");
                goto ERROR;
            }

            Release(id);

            if ((id = StringNew((const char *) this->tkcur_.buf)) == nullptr) {
                Release(paths);
                return nullptr;
            }

            end = this->tkcur_.end;
            this->Eat();
        }
    }

    if ((ret = ArObjectNew<Binary>(RCType::INLINE, type_ast_import_name_)) == nullptr) {
        Release(paths);
        Release(id);
        return nullptr;
    }

    ret->start = start;
    ret->end = end;
    ret->lineno = 0;
    ret->colno = 0;

    ret->left = paths;
    ret->right = id;

    return ret;

    ERROR:
    Release(id);
    Release(paths);
    Release(scope_sep);
    return nullptr;
}

Node *Parser::ParseDecls(bool nostatic) {
    Node *stmt;
    Pos start = this->tkcur_.start;
    bool gbl_nostatic = this->no_static_;
    bool pub = false;

    if (this->MatchEat(TokenType::PUB, false))
        pub = true;

    if (nostatic || gbl_nostatic)
        this->no_static_ = true;

    switch (this->tkcur_.type) {
        case TokenType::WEAK:
        case TokenType::VAR:
            stmt = this->ParseVarDecl(false, pub);
            break;
        case TokenType::LET:
            if (this->no_static_)
                return (Node *) ErrorFormat(type_syntax_error_, "unexpected use of 'let' in this context");

            stmt = this->ParseVarDecl(true, pub);
            break;
        case TokenType::FUNC:
            stmt = this->FuncDecl(pub, false);
            break;
        case TokenType::STRUCT:
            if (this->no_static_)
                return (Node *) ErrorFormat(type_syntax_error_, "unexpected struct declaration");

            stmt = this->StructDecl(pub);
            break;
        case TokenType::TRAIT:
            if (this->no_static_)
                return (Node *) ErrorFormat(type_syntax_error_, "unexpected trait declaration");

            stmt = this->TraitDecl(pub);
            break;
        default:
            if (pub)
                return (Node *) ErrorFormat(type_syntax_error_, "expected declaration after 'pub' keyword");

            stmt = this->ParseStatement();
    }

    if (stmt != nullptr && pub)
        stmt->start = start;

    this->no_static_ = gbl_nostatic;

    return stmt;
}

NudMeth Parser::LookupNud() {
    if (this->IsLiteral())
        return &Parser::ParseLiteral;

    switch (this->tkcur_.type) {
        case TokenType::IDENTIFIER:
        case TokenType::BLANK:
        case TokenType::SELF:
            return &Parser::ParseIdentifier;
        case TokenType::LEFT_SQUARE:
            return &Parser::ParseList;
        case TokenType::LEFT_BRACES:
            return &Parser::ParseMapSet;
        case TokenType::LEFT_ROUND:
            return &Parser::ParseTupleLambda;
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
        case TokenType::MINUS:
        case TokenType::PLUS:
            return &Parser::ParsePrefix;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            return &Parser::ParsePreUpdate;
        default:
            return nullptr;
    }
}

void Parser::Eat() {
    if (this->tkcur_.type == TokenType::END_OF_FILE)
        return;

    this->tkcur_ = this->scanner_->NextToken();

    while (this->tkcur_.type == TokenType::INLINE_COMMENT || this->tkcur_.type == TokenType::COMMENT)
        this->tkcur_ = this->scanner_->NextToken();
}

void Parser::EatTerm(TokenType stop) {
    if (!this->Match(stop)) {
        while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON))
            this->Eat();
    }
}

Parser::Parser(Scanner *scanner, const char *filename) {
    this->scanner_ = scanner;
    this->filename_ = filename;
    this->no_static_ = false;
}

File *Parser::Parse() {
    File *program;
    List *decls;
    Node *tmp;

    if ((decls = ListNew()) == nullptr)
        return nullptr;

    // Initialize parser
    this->Eat();

    this->no_init_ = false;

    while (!this->MatchEat(TokenType::END_OF_FILE, true)) {
        if ((tmp = this->ParseDecls(false)) == nullptr) {
            Release(decls);
            return nullptr;
        }

        if (!ListAppend(decls, tmp)) {
            Release(decls);
            return nullptr;
        }

        Release(tmp);
    }

    if ((program = ArObjectNew<File>(RCType::INLINE, type_ast_file_)) == nullptr)
        return nullptr;

    if ((program->name = StringNew(this->filename_)) == nullptr) {
        Release(program);
        Release(decls);
        return nullptr;
    }

    program->start = 1;
    program->end = this->tkcur_.end;
    program->lineno = 0;
    program->colno = 0;

    program->decls = decls;

    return program;
}

#undef EXPR_NO_ASSIGN
#undef EXPR_NO_LIST
