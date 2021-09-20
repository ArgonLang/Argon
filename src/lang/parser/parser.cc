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

using namespace argon::object;
using namespace argon::lang::scanner2;
using namespace argon::lang::parser;

int PeekPrecedence(TokenType type) {
    switch (type) {
        case TokenType::ELVIS:
        case TokenType::QUESTION:
            return 5;
        case TokenType::OR:
            return 10;
        case TokenType::AND:
            return 20;
        case TokenType::PIPE:
            return 30;
        case TokenType::CARET:
            return 40;
        case TokenType::EQUAL_EQUAL:
        case TokenType::NOT_EQUAL:
            return 50;
        case TokenType::LESS:
        case TokenType::LESS_EQ:
        case TokenType::GREATER:
        case TokenType::GREATER_EQ:
            return 60;
        case TokenType::SHL:
        case TokenType::SHR:
            return 70;
        case TokenType::PLUS:
        case TokenType::MINUS:
            return 80;
        case TokenType::ASTERISK:
        case TokenType::SLASH:
        case TokenType::SLASH_SLASH:
        case TokenType::PERCENT:
            return 90;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
        case TokenType::LEFT_SQUARE:
        case TokenType::DOT:
        case TokenType::QUESTION_DOT:
            return 100;
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
    if (!this->Match(type)) {
        if (eat_nl) {
            while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON))
                this->Eat();

            if (this->Match(type)) {
                while (this->Match(TokenType::END_OF_LINE, TokenType::SEMICOLON))
                    this->Eat();

                return true;
            }
        }

        return false;
    }

    this->Eat();
    return true;
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
        case TokenType::ELVIS:
            return &Parser::ParseElvis;
        case TokenType::QUESTION:
            return &Parser::ParseTernary;
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

    *is_kwargs = true;

    if (!this->MatchEat(TokenType::RIGHT_BRACES, true)) {
        *is_kwargs = false;
        do {
            count++;
            while (true) {
                if ((tmp = this->ParseExpr()) == nullptr)
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

Node *Parser::Expression() {
    Node *left;
    Node *right;
    Node *ret;

    TokenType kind;

    if ((left = this->ParseTestList()) == nullptr)
        return nullptr;

    kind = this->tkcur_.type;

    if (this->TokenInRange(TokenType::ASSIGNMENT_BEGIN, TokenType::ASSIGNMENT_END)) {
        this->Eat();

        if (!AR_TYPEOF(left, type_ast_identifier_) && !AR_TYPEOF(left, type_ast_list_)) {
            Release(left);
            return (Node *) ErrorFormat(type_syntax_error_,
                                        "expected identifier to the left of the assignment expression");
        }

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

        if ((right = this->ParseTestList()) == nullptr) {
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

    // expression
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

Node *Parser::ParseList() {
    Pos start = this->tkcur_.start;
    List *list;
    Node *tmp;

    this->Eat();

    if ((list = ListNew()) == nullptr)
        return nullptr;

    if (!this->MatchEat(TokenType::RIGHT_SQUARE, true)) {
        do {
            if ((tmp = this->ParseExpr()) == nullptr)
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

Node *Parser::ParseTestList() {
    List *list = nullptr;
    Node *tmp;
    Pos start;
    Pos end;

    tmp = this->ParseExpr();
    start = tmp->start;
    end = tmp->end;

    while (this->MatchEat(TokenType::COMMA, true)) {
        if (list == nullptr) {
            if ((list = ListNew()) == nullptr) {
                Release(tmp);
                return nullptr;
            }
        } else
            tmp = this->ParseExpr();

        if (!ListAppend(list, tmp)) {
            Release(tmp);
            Release(list);
            return nullptr;
        }

        end = tmp->end;
        Release(tmp);
    }

    if ((tmp = ArObjectNew<Unary>(RCType::INLINE, type_ast_list_)) == nullptr) {
        Release(list);
        return nullptr;
    }

    tmp->start = start;
    tmp->end = end;
    tmp->colno = 0;
    tmp->lineno = 0;
    ((Unary *) tmp)->value = list;

    return tmp;
}

Node *Parser::ParseTupleLambda() {
    Pos start = this->tkcur_.start;
    Pos end;
    bool must_fn;
    bool last_is_comma;

    List *args;
    Node *tmp;

    this->Eat();

    if ((args = ListNew()) == nullptr)
        return nullptr;

    must_fn = false;
    if (!this->MatchEat(TokenType::RIGHT_ROUND, true)) {
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
                    tmp = this->ParseExpr();

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
            if (!AR_TYPEOF(args->objects[i], type_ast_identifier_)) {
                Release(args);
                return (Node *) ErrorFormat(type_syntax_error_,
                                            "expected identifier as %d element in arrow definition", i);
            }
        }

        assert(false); // TODO; function def
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

Parser::Parser(Scanner *scanner, const char *filename) {
    this->scanner_ = scanner;
    this->filename_ = filename;
}

File *Parser::Parse() {
    // Initialize parser
    this->Eat();

    Node *node = this->Expression();

    return nullptr;
}
