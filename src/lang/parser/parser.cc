// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/bool.h>
#include <object/datatype/bytes.h>
#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/list.h>
#include <object/datatype/nil.h>
#include <object/datatype/string.h>

#include "parser.h"

#define EXPR_NO_ASSIGN      11
#define EXPR_NO_LIST        21
#define EXPR_NO_STRUCT_INIT 31

using namespace argon::object;
using namespace argon::lang::scanner2;
using namespace argon::lang::parser;

bool IsIdentifiersList(Node *node) {
    auto *ast_list = (Unary *) node;
    auto *list = (List *) ast_list->value;

    if (!AR_TYPEOF(node, type_ast_list_))
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
        case TokenType::OR:
            return 50;
        case TokenType::AND:
            return 60;
        case TokenType::PIPE:
            return 70;
        case TokenType::CARET:
            return 80;
        case TokenType::EQUAL_EQUAL:
        case TokenType::NOT_EQUAL:
            return 90;
        case TokenType::LESS:
        case TokenType::LESS_EQ:
        case TokenType::GREATER:
        case TokenType::GREATER_EQ:
            return 100;
        case TokenType::SHL:
        case TokenType::SHR:
            return 110;
        case TokenType::PLUS:
        case TokenType::MINUS:
            return 120;
        case TokenType::ASTERISK:
        case TokenType::SLASH:
        case TokenType::SLASH_SLASH:
        case TokenType::PERCENT:
            return 130;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
        case TokenType::LEFT_SQUARE:
        case TokenType::LEFT_ROUND:
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
            return 140;
        default:
            return 1000;
    }
}

inline bool Parser::IsLiteral() {
    return this->TokenInRange(TokenType::NUMBER_BEGIN, TokenType::NUMBER_END)
           || this->TokenInRange(TokenType::STRING_BEGIN, TokenType::STRING_END)
           || this->Match(TokenType::TRUE)
           || this->Match(TokenType::FALSE)
           || this->Match(TokenType::NIL);
}

bool Parser::MatchEat(TokenType type, bool eat_nl) {
    bool match;

    if (eat_nl) {
        while (this->Match(TokenType::END_OF_LINE))
            this->Eat();
    }

    if ((match = this->Match(type)))
        this->Eat();

    if (eat_nl) {
        while (this->Match(TokenType::END_OF_LINE))
            this->Eat();
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
                return true;
            }

            id->start = start;
            id->end = this->tkcur_.end;
            id->colno = 0;
            id->lineno = 0;
            id->value = str;

            *rest_node = id;
        }

        this->Eat();
        return true;
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

Node *Parser::ParseAssignment(Node *left) {
    TokenType kind = this->tkcur_.type;
    Node *right;
    Binary *ret;

    this->Eat();

    if (!AR_TYPEOF(left, type_ast_identifier_) && !AR_TYPEOF(left, type_ast_list_))
        return (Node *) ErrorFormat(type_syntax_error_,
                                    "expected identifier or list to the left of the assignment expression");

    if (AR_TYPEOF(left, type_ast_list_)) {
        auto *list = (List *) ((Unary *) left)->value;

        for (int i = 0; i < list->len; i++) {
            if (!AR_TYPEOF(list->objects[i], type_ast_identifier_)) {
                Release(left);
                return (Node *) ErrorFormat(type_syntax_error_,
                                            "expected identifier as %d element in assignment definition", i);
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

Node *Parser::ParseBlock() {
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
        if ((tmp = this->SmallDecl(false)) == nullptr) {
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
                tmp = this->FuncDecl(pub);
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
    ArObject *tmp;
    List *scopes;
    Unary *ret;

    Pos start;
    Pos end;

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier");

    if ((tmp = StringNew((const char *) this->tkcur_.buf)) == nullptr)
        return nullptr;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->Match(TokenType::SCOPE)) {
        if ((ret = ArObjectNew<Unary>(RCType::INLINE, type_ast_identifier_)) == nullptr) {
            Release(tmp);
            return nullptr;
        }

        ret->start = this->tkcur_.start;
        ret->end = this->tkcur_.end;
        ret->colno = 0;
        ret->lineno = 0;
        ret->value = tmp;

        return ret;
    }

    if ((scopes = ListNew()) == nullptr) {
        Release(tmp);
        return nullptr;
    }

    if (!ListAppend(scopes, tmp))
        goto ERROR;

    Release(tmp);

    this->Eat(); // Eat ::

    do {
        if (!this->Match(TokenType::IDENTIFIER)) {
            ErrorFormat(type_syntax_error_, "expected identifier after '::'");
            goto ERROR;
        }

        if ((tmp = StringNew((const char *) this->tkcur_.buf)) == nullptr)
            goto ERROR;

        if (!ListAppend(scopes, tmp))
            goto ERROR;

        Release(tmp);

        this->Eat();

        end = this->tkcur_.end;
    } while (this->MatchEat(TokenType::SCOPE, false));

    if ((ret = ArObjectNew<Unary>(RCType::INLINE, type_ast_scope_)) == nullptr)
        goto ERROR;

    ret->start = start;
    ret->end = end;
    ret->colno = 0;
    ret->lineno = 0;
    ret->value = scopes;

    return ret;

    ERROR:
    Release(tmp);
    Release(scopes);
    return nullptr;

}

Node *Parser::Expression() {
    Node *left;
    Node *ret;

    if ((left = this->ParseExpr()) == nullptr)
        return nullptr;

    if (this->Match(TokenType::COLON))
        return left; // Return identifier, this is a label!

    if (!AR_TYPEOF(left, type_ast_assignment_)) {
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

Node *Parser::ParseVarDecl(bool constant, bool pub) {
    Assignment *assign = nullptr;
    Node *tmp = nullptr;
    List *lets = nullptr;
    bool weak = false;
    int index = 0;
    Pos start;
    Pos end;

    if (!constant && this->MatchEat(TokenType::WEAK, false))
        weak = true;

    start = this->tkcur_.start;
    this->Eat();

    do {
        if (assign != nullptr) {
            if (lets == nullptr && (lets = ListNew()) == nullptr) {
                Release(assign);
                return nullptr;
            }

            if (!ListAppend(lets, assign))
                goto ERROR;

            Release(assign);
            assign = nullptr;
        }

        if (!this->Match(TokenType::IDENTIFIER)) {
            ErrorFormat(type_syntax_error_, "expected identifier after %s keyword", constant ? "let" : "var");
            goto ERROR;
        }

        if ((assign = AssignmentNew(this->tkcur_, constant, pub, weak)) == nullptr)
            goto ERROR;

        end = this->tkcur_.end;
        this->Eat();
    } while (this->MatchEat(TokenType::COMMA, true));

    if (lets != nullptr) {
        if (!ListAppend(lets, assign))
            goto ERROR;

        Release(assign);
        assign = nullptr;
    }

    if (this->MatchEat(TokenType::EQUAL, true)) {
        do {
            if ((tmp = this->ParseExpr(EXPR_NO_LIST)) == nullptr)
                goto ERROR;

            if (lets == nullptr) {
                assign->start = start;
                assign->end = tmp->end;
                assign->value = tmp;
                break;
            }

            if (index >= lets->len) {
                ErrorFormat(type_syntax_error_, "values to be assigned exceeded of: %d", index - lets->len);
                goto ERROR;
            }

            end = tmp->end;
            ((Assignment *) lets->objects[index])->end = tmp->end;
            ((Assignment *) lets->objects[index++])->value = tmp;
        } while (this->MatchEat(TokenType::COMMA, true));
    } else {
        if (constant) {
            ErrorFormat(type_syntax_error_, "expected = after identifier/s in let declaration");
            goto ERROR;
        }
    }

    if (assign != nullptr)
        return assign;

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_list_decl_)) == nullptr)
        goto ERROR;

    tmp->start = start;
    tmp->end = end;
    tmp->colno = 0;
    tmp->lineno = 0;
    ((Unary *) tmp)->value = lets;

    return tmp;

    ERROR:
    Release(lets);
    Release(assign);
    Release(tmp);
    return nullptr;
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

        if ((led = this->LookupLed()) == nullptr)
            break;

        if ((right = (this->*led)(left)) == nullptr) {
            Release(left);
            return nullptr;
        }

        left = right;
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

    if (!this->MatchEat(TokenType::RIGHT_ROUND, true)) {
        ErrorFormat(type_syntax_error_, "expected ')' after last argument of function call");
        goto ERROR;
    }

    if ((tmp = ArObjectNew<Binary>(RCType::INLINE, type_ast_call_)) == nullptr) {
        Release(args);
        return nullptr;
    }

    tmp->start = left->start;
    tmp->end = end;
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

        if (init == nullptr || (!AR_TYPEOF(init, type_ast_identifier_) && !AR_TYPEOF(init, type_ast_list_))) {
            Release(init);
            return (Node *) ErrorFormat(type_syntax_error_, "expected identifier or tuple before 'in'");
        }

        if (!IsIdentifiersList(init)) {
            Release(init);
            return (Node *) ErrorFormat(type_syntax_error_, "expected identifiers list");
        }
    } else {
        if (!this->MatchEat(TokenType::SEMICOLON, true)) {
            Release(init);
            return (Node *) ErrorFormat(type_syntax_error_, "expected ';' after for 'init'");
        }
    }

    if (type == type_ast_for_) {
        if ((test = this->ParseExpr(EXPR_NO_STRUCT_INIT)) == nullptr)
            goto ERROR;

        if (!this->MatchEat(TokenType::SEMICOLON, true)) {
            ErrorFormat(type_syntax_error_, "expected ';' after for 'test'");
            goto ERROR;
        }

        if ((inc = this->ParseExpr(EXPR_NO_STRUCT_INIT)) == nullptr)
            goto ERROR;
    } else {
        if ((test = this->ParseExpr(EXPR_NO_STRUCT_INIT)) == nullptr)
            goto ERROR;
    }

    if ((body = this->ParseBlock()) == nullptr)
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

Node *Parser::FuncDecl(bool pub) {
    String *name = nullptr;
    List *params = nullptr;
    Node *tmp = nullptr;
    Construct *fn;
    Pos start;
    bool exit;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after 'func' keyword");

    if ((name = StringNew((const char *) this->tkcur_.buf)) == nullptr)
        return nullptr;

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

        if (!this->MatchEat(TokenType::RIGHT_ROUND, true)) {
            ErrorFormat(type_syntax_error_, "expected ')' after function params");
            goto ERROR;
        }
    }

    if ((tmp = this->ParseBlock()) == nullptr)
        goto ERROR;

    if ((fn = FunctionNew(start, name, params, tmp, pub)) == nullptr)
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
    ArObject *str;

    if ((id = ArObjectNew<Unary>(RCType::INLINE, type_ast_identifier_)) != nullptr) {
        if ((str = StringNew((const char *) this->tkcur_.buf)) == nullptr) {
            Release(id);
            return nullptr;
        }

        id->start = this->tkcur_.start;
        id->end = this->tkcur_.end;
        id->colno = 0;
        id->lineno = 0;
        id->value = str;
    }

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

    if (!this->Match(TokenType::RIGHT_SQUARE)) {
        this->EatTerm();
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

    if (!this->MatchEat(TokenType::RIGHT_SQUARE, true)) {
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
            // TODO: number_chr
            assert(false);
        case TokenType::STRING:
        case TokenType::RAW_STRING:
            value = StringNew((const char *) this->tkcur_.buf);
            break;
        case TokenType::BYTE_STRING:
            value = BytesNew(this->tkcur_.buf, std::strlen((const char *) this->tkcur_.buf), true);
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
        if ((loop->body = this->ParseBlock()) == nullptr) {
            Release(loop);
            return nullptr;
        }
    } else {
        if ((loop->test = this->ParseExpr(EXPR_NO_STRUCT_INIT)) == nullptr) {
            Release(loop);
            return nullptr;
        }

        if ((loop->body = this->ParseBlock()) == nullptr) {
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

    this->Eat();

    if ((test->test = this->ParseExpr(EXPR_NO_STRUCT_INIT)) == nullptr)
        goto ERROR;

    if ((test->body = this->ParseBlock()) == nullptr)
        goto ERROR;

    test->end = ((Node *) test->body)->end;

    if (this->MatchEat(TokenType::ELIF, false)) {
        if ((test->orelse = this->ParseIf()) == nullptr)
            goto ERROR;

        test->end = ((Node *) test->orelse)->end;
    } else if (this->MatchEat(TokenType::ELSE, false)) {
        if ((test->orelse = this->ParseBlock()) == nullptr)
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
    Binary *binary;
    Node *right;

    this->Eat();

    if (!this->Match(TokenType::IDENTIFIER))
        return (Node *) ErrorFormat(type_syntax_error_, "expected identifier after '.'/'?.'/'::' access operator");

    if ((right = this->ParseExpr(PeekPrecedence(kind))) == nullptr)
        return nullptr;

    if ((binary = ArObjectNew<Binary>(RCType::INLINE, type_ast_selector_)) == nullptr)
        Release(right);

    binary->kind = kind;
    binary->start = left->start;
    binary->end = right->end;
    binary->colno = 0;
    binary->lineno = 0;
    binary->left = left;
    binary->right = right;

    return binary;
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
    Unary *unary;
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

    if ((unary = UnaryNew(kind, start, right)) == nullptr)
        Release(right);

    return unary;
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

Node *Parser::ParseDecls() {
    Pos start = this->tkcur_.start;
    bool pub = false;
    Node *stmt;

    if (this->MatchEat(TokenType::PUB, false))
        pub = true;

    switch (this->tkcur_.type) {
        case TokenType::LET:
            stmt = this->ParseVarDecl(true, pub);
            break;
        case TokenType::TRAIT:
            stmt = this->TraitDecl(pub);
            break;
        default:
            stmt = this->SmallDecl(pub);
    }

    if (stmt != nullptr && pub)
        stmt->start = start;

    return stmt;
}

Node *Parser::ParseStatement() {
    Pos start = this->tkcur_.start;
    Node *label = nullptr;
    Node *tmp;
    Binary *ret;

    do {
        if (this->TokenInRange(TokenType::KEYWORD_BEGIN, TokenType::KEYWORD_END)) {
            switch (this->tkcur_.type) {
                case TokenType::DEFER:
                case TokenType::SPAWN:
                    tmp = this->ParseOOBCall();
                    break;
                case TokenType::RETURN:
                    tmp = this->ParseReturn();
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
                    break;
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

    Pos start;
    Pos end;

    start = this->tkcur_.start;

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
        if ((body = ListNew()) == nullptr)
            goto ERROR;

        if ((tmp = this->SmallDecl(false)) == nullptr)
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
    if (last_fallthrough != -1 && last_fallthrough != body->len - 1) {
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
    Binary *ret;
    Pos start;
    Pos end;

    bool def = false;

    start = this->tkcur_.start;
    this->Eat();

    if (!this->Match(TokenType::LEFT_BRACES))
        if ((test = this->ParseExpr()) == nullptr)
            return nullptr;

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

    if (this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        ErrorFormat(type_syntax_error_, "expected '}' after switch declaration");
        goto ERROR;
    }

    if ((ret = ArObjectNew<Binary>(RCType::INLINE, type_ast_switch_)) == nullptr)
        goto ERROR;

    ret->start = start;
    ret->end = end;
    ret->colno = 0;
    ret->lineno = 0;

    ret->left = test;
    ret->right = cases;

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
    Test *test;

    Node *body;
    Node *orelse;

    this->Eat();

    if ((body = this->ParseExpr()) == nullptr)
        return nullptr;

    if (!this->MatchEat(TokenType::COLON, false)) {
        Release(body);
        return (Node *) ErrorFormat(type_syntax_error_, "expected : in ternary operator");
    }

    if ((orelse = this->ParseExpr()) == nullptr) {
        Release(body);
        return nullptr;
    }

    if ((test = ArObjectNew<Test>(RCType::INLINE, type_ast_test_)) == nullptr) {
        Release(body);
        Release(orelse);
        return nullptr;
    }

    test->start = start;
    test->end = orelse->end;
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

    if ((ret = ArObjectNew<Unary>(RCType::INLINE, type_ast_list_)) == nullptr) {
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

        if ((tmp = this->ParseBlock()) == nullptr) {
            Release(args);
            return nullptr;
        }

        if ((fn = FunctionNew(start, nullptr, args, tmp, false)) == nullptr)
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
    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_list_)) == nullptr) {
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
    Binary *ret;
    String *paths;
    String *scope_sep;
    String *tmp;

    Pos start;
    Pos end;

    if (!this->Match(TokenType::IDENTIFIER)) {
        if (id_only)
            return (Node *) ErrorFormat(type_syntax_error_, "expected name");
        return (Node *) ErrorFormat(type_syntax_error_, "expected name or scope path");
    }

    if ((paths = StringNew((const char *) this->tkcur_.buf)) == nullptr)
        return nullptr;

    start = this->tkcur_.start;
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

Node *Parser::SmallDecl(bool pub) {
    switch (this->tkcur_.type) {
        case TokenType::WEAK:
        case TokenType::VAR:
            return this->ParseVarDecl(false, pub);
        case TokenType::FUNC:
            return this->FuncDecl(pub);
        case TokenType::STRUCT:
            return this->StructDecl(pub);
        default:
            if (pub)
                return (Node *) ErrorFormat(type_syntax_error_, "expected declaration after 'pub' keyword");
            return this->ParseStatement();
    }
}

NudMeth Parser::LookupNud() {
    if (this->IsLiteral())
        return &Parser::ParseLiteral;

    switch (this->tkcur_.type) {
        case TokenType::IDENTIFIER:
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
    this->tkcur_ = this->scanner_->NextToken();
}

void Parser::EatTerm() {
    while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON))
        this->Eat();
}

Parser::Parser(Scanner *scanner, const char *filename) {
    this->scanner_ = scanner;
    this->filename_ = filename;
}

File *Parser::Parse() {
    File *program;
    List *decls;
    Node *tmp;

    if ((decls = ListNew()) == nullptr)
        return nullptr;

    // Initialize parser
    this->Eat();

    while (!this->MatchEat(TokenType::END_OF_FILE, true)) {
        if ((tmp = this->ParseDecls()) == nullptr) {
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
#undef EXPR_NO_STRUCT_INIT
