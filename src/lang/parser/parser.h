// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_PARSER_H_
#define ARGON_LANG_PARSER_PARSER_H_

#include <lang/scanner/scanner.h>

#include "node.h"

namespace argon::lang::parser {
    using PFlag = int;

    class Parser {
        using NudMeth = Node *(Parser::*)();
        using LedMeth = Node *(Parser::*)(PFlag flags, Node *);

        lang::scanner::Scanner &scanner_;

        const char *filename_;

        lang::scanner::Token tkcur_;

        [[nodiscard]] bool Match(scanner::TokenType type) const {
            return this->tkcur_.type == type;
        }

        template<typename ...TokenTypes>
        [[nodiscard]] bool Match(scanner::TokenType type, TokenTypes... types) const {
            if (!this->Match(type))
                return this->Match(types...);
            return true;
        }

        bool MatchEat(scanner::TokenType type) {
            if (this->Match(type)) {
                this->Eat();
                return true;
            }

            return false;
        }

        [[nodiscard]] bool TokenInRange(scanner::TokenType begin, scanner::TokenType end) const {
            return this->tkcur_.type > begin && this->tkcur_.type < end;
        }

        static int PeekPrecedence(scanner::TokenType token);

        [[nodiscard]] LedMeth LookupLed(lang::scanner::TokenType token) const;

        Node *ParseExpression(PFlag flags, int precedence);

        Node *ParseIdentifier();

        Node *ParseInfix(PFlag flags, Node *left);

        Node *ParseList();

        Node *ParseLiteral();

        Node *ParsePrefix();

        [[nodiscard]] NudMeth LookupNud(lang::scanner::TokenType token) const;

        void Eat();

        void EatNL();

        void IgnoreNL();

    public:
        Parser(const char *filename, lang::scanner::Scanner &scanner) noexcept: scanner_(scanner),
                                                                                filename_(filename) {}

        File *Parse() noexcept;
    };

} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSER_H_
