// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_PARSER_H_
#define ARGON_LANG_PARSER_PARSER_H_

#include <lang/scanner/scanner2.h>

#include "nodes.h"

namespace argon::lang::parser {
    class Parser;

    using NudMeth = Node *(Parser::*)();
    using LedMeth = Node *(Parser::*)(Node *left);

    class Parser {
        scanner2::Scanner *scanner_;
        const char *filename_;

        scanner2::Token tkcur_;

        bool IsLiteral();

        [[nodiscard]] bool Match(scanner2::TokenType type) const {
            return this->tkcur_.type == type;
        }

        template<typename ...TokenTypes>
        [[nodiscard]] bool Match(scanner2::TokenType type, TokenTypes... types) const {
            if (!this->Match(type))
                return this->Match(types...);
            return true;
        }

        [[nodiscard]] bool MatchEat(scanner2::TokenType type, bool eat_nl);

        [[nodiscard]] bool TokenInRange(scanner2::TokenType begin, scanner2::TokenType end) const;

        [[nodiscard]] bool ParseRestElement(Node **rest_node);

        [[nodiscard]] LedMeth LookupLed() const;

        [[nodiscard]] argon::object::ArObject *
        ParseArgsKwargs(const char *partial_error, const char *error, bool *is_kwargs);

        [[nodiscard]] Node *Expression();

        [[nodiscard]] Node *ParseElvis(Node *left);

        [[nodiscard]] Node *ParseExpr(int precedence);

        [[nodiscard]] inline Node *ParseExpr() { return this->ParseExpr(0); }

        [[nodiscard]] Node *ParseIdentifier();

        [[nodiscard]] Node *ParseList();

        [[nodiscard]] Node *ParseLiteral();

        [[nodiscard]] Node *ParseInfix(Node *left);

        [[nodiscard]] Node *ParseInitialization(Node *left);

        [[nodiscard]] Node *ParseMapSet();

        [[nodiscard]] Node *ParseMemberAccess(Node *left);

        [[nodiscard]] Node *ParsePostUpdate(Node *left);

        [[nodiscard]] Node *ParsePreUpdate();

        [[nodiscard]] Node *ParsePrefix();

        [[nodiscard]] Node *ParseSubscript(Node *left);

        [[nodiscard]] Node *ParseTernary(Node *left);

        [[nodiscard]] Node *ParseTestList();

        [[nodiscard]] Node *ParseTupleLambda();

        [[nodiscard]] NudMeth LookupNud();

        void Eat();

    public:
        Parser(scanner2::Scanner *scanner, const char *filename);

        File *Parse();
    };


}  // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSER_H_
