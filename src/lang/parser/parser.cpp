// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/atom.h>
#include <vm/datatype/boolean.h>
#include <vm/datatype/list.h>
#include <vm/datatype/nil.h>

#include "parsererr.h"
#include "parser.h"

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

int Parser::PeekPrecedence(scanner::TokenType token) {
    switch (token) {
        case TokenType::LEFT_BRACES:
            return 10;
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
            return 20;
        case TokenType::LEFT_SQUARE:
            return 30;
        default:
            return 1000;
    }
}

Parser::LedMeth Parser::LookupLed(lang::scanner::TokenType token) const {
    if (this->TokenInRange(TokenType::INFIX_BEGIN, TokenType::INFIX_END))
        return &Parser::ParseInfix;

    return nullptr;
}

Parser::NudMeth Parser::LookupNud(lang::scanner::TokenType token) const {
    if (this->TokenInRange(TokenType::LITERAL_BEGIN, TokenType::LITERAL_END))
        return &Parser::ParseLiteral;

    switch (token) {
        case TokenType::IDENTIFIER:
        case TokenType::BLANK:
        case TokenType::SELF:
            return &Parser::ParseIdentifier;
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::EXCLAMATION:
        case TokenType::TILDE:
            return &Parser::ParsePrefix;
        case TokenType::LEFT_SQUARE:
            return &Parser::ParseList;
        case TokenType::LEFT_BRACES:
            return &Parser::ParseDictSet;
        default:
            return nullptr;
    }
}

Node *Parser::ParseExpression(PFlag flags, int precedence) {
    LedMeth led;
    NudMeth nud;

    ARC left;

    if ((nud = this->LookupNud(TKCUR_TYPE)) == nullptr)
        throw ParserException("invalid token");

    left = (ArObject *) (this->*nud)();

    while (precedence < Parser::PeekPrecedence(TKCUR_TYPE)) {
        if ((led = this->LookupLed(TKCUR_TYPE)) == nullptr)
            break;

        left = (ArObject *) (this->*led)(flags, (Node *) left.Get());
    }

    return (Node *) left.Unwrap();
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

Node *Parser::ParseInfix(PFlag flags, Node *left) {
    TokenType kind = TKCUR_TYPE;
    ARC right;

    this->Eat();

    right = (ArObject *) this->ParseExpression(flags, this->PeekPrecedence(kind));

    auto *binary = BinaryNew(left, (Node *) right.Get(), kind, NodeType::BINARY);
    if (binary == nullptr)
        throw DatatypeException();

    return (Node *) binary;
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

            auto *itm = this->ParseExpression(0, 0);

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

    //TODO: Missing some literals

    switch (this->tkcur_.type) {
        case TokenType::ATOM:
            value = (ArObject *) AtomNew((const char *) this->tkcur_.buffer);
            break;
        case TokenType::NUMBER:
            //value = IntegerNew((const char *) this->tkcur_.buf, 10);
            break;
        case TokenType::NUMBER_BIN:
            //value = IntegerNew((const char *) this->tkcur_.buf, 2);
            break;
        case TokenType::NUMBER_OCT:
            //value = IntegerNew((const char *) this->tkcur_.buf, 8);
            break;
        case TokenType::NUMBER_HEX:
            //value = IntegerNew((const char *) this->tkcur_.buf, 16);
            break;
        case TokenType::DECIMAL:
            //value = DecimalNew((const char *) this->tkcur_.buf);
            break;
        case TokenType::NUMBER_CHR:
            //value = IntegerNew(StringUTF8toInt((const unsigned char *) this->tkcur_.buf));
            break;
        case TokenType::STRING:
        case TokenType::RAW_STRING:
            value = (ArObject *) StringNew((const char *) this->tkcur_.buffer, this->tkcur_.length);
            break;
        case TokenType::BYTE_STRING:
            //value = BytesNew(this->tkcur_.buf, this->tkcur_.buflen, true);
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

        auto *expr = this->ParseExpression(0, 0);

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

            expr = this->ParseExpression(0, 0);

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

Node *Parser::ParsePrefix() {
    Position start = this->tkcur_.loc.start;
    TokenType kind = TKCUR_TYPE;
    Unary *unary;

    this->Eat();

    auto *right = this->ParseExpression(0, PeekPrecedence(kind));

    unary = UnaryNew((ArObject *) right, kind, right->loc);

    Release(right);

    if (unary == nullptr)
        throw DatatypeException();

    unary->loc.start = start;

    return (Node *) unary;
}

void Parser::Eat() {
    if (this->tkcur_.type == TokenType::END_OF_FILE)
        return;

    if (!this->scanner_.NextToken(&this->tkcur_))
        throw ScannerException();
}

void Parser::EatNL() {
    this->IgnoreNL();
    this->Eat();
    this->IgnoreNL();
}

void Parser::IgnoreNL() {
    while (this->Match(TokenType::END_OF_LINE))
        this->Eat();
}

// PUBLIC

File *Parser::Parse() noexcept {
    ARC result;

    Position start{};
    Position end{};

    List *statements;

    if ((statements = ListNew()) == nullptr)
        return nullptr;

    try {
        this->Eat();

        start = this->tkcur_.loc.start;

        while (!this->Match(TokenType::END_OF_FILE)) {
            result = (ArObject *) this->ParseExpression(0, 0);

            if (!ListAppend(statements, result.Get())) {
                Release(statements);
                return nullptr;
            }

            end = this->tkcur_.loc.end;
        }
    } catch (const ParserException &) {
        assert(false);
    }

    auto *file = FileNew(this->filename_, statements);
    if (file != nullptr) {
        file->loc.start = start;
        file->loc.end = end;
    }

    Release(statements);

    return file;
}