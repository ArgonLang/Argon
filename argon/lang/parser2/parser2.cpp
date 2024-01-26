// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/exception.h>
#include <argon/lang/parser2/parser2.h>

using namespace argon::lang::scanner;
using namespace argon::lang::parser2;
using namespace argon::lang::parser2::node;

#define TKCUR_TYPE this->tkcur_.type

Node *Parser::ParseDecls(Context *context) {
    bool pub = false;

    if (this->MatchEat(TokenType::KW_PUB, true)) {
        pub = true;

        if (!this->CheckScope(context, ContextType::MODULE, ContextType::STRUCT, ContextType::TRAIT))
            throw ParserException(this->tkcur_.loc,
                                  "'pub' modifier not allowed in %s",
                                  kContextName[(int) context->type]);
    }

    switch (TKCUR_TYPE) {
        case TokenType::KW_ASYNC:
        case TokenType::KW_FROM:
        case TokenType::KW_FUNC:
        case TokenType::KW_IMPORT:
            break;
        case TokenType::KW_LET:
        case TokenType::KW_STRUCT:
        case TokenType::KW_SYNC:
        case TokenType::KW_TRAIT:
        case TokenType::KW_VAR:
        case TokenType::KW_WEAK:
        default:
            break;
    }

    return nullptr;
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