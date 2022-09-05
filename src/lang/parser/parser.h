// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_PARSER_H_
#define ARGON_LANG_PARSER_PARSER_H_

#include <lang/scanner/scanner.h>

#include "node.h"

namespace argon::lang::parser {
    using PFlag = int;

    enum class ParserScope {
        BLOCK,
        LOOP,
        MODULE,
        STRUCT,
        SWITCH,
        TRAIT
    };

    class Parser {
        using NudMeth = Node *(Parser::*)();
        using LedMeth = Node *(Parser::*)(Node *);

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

        argon::vm::datatype::ArObject *ParseParamList(bool parse_expr);

        argon::vm::datatype::ArObject *ParseTraitList();

        Node *ParseAssertion();

        Node *ParseAssignment(Node *left);

        Node *ParseAsync(ParserScope scope, bool pub);

        Node *ParseAsync();

        Node *ParseArrowOrTuple();

        Node *ParseBCFLabel();

        Node *ParseBlock(ParserScope scope);

        Node *ParseDecls(ParserScope scope);

        Node *ParseDictSet();

        Node *ParseElvis(Node *left);

        Node *ParseExpression();

        Node *ParseExpression(int precedence);

        Node *ParseExpressionList(Node *left);

        Node *ParseFor();

        Node *ParseFn(ParserScope scope, bool pub);

        Node *ParseFnCall(Node *left);

        Node *ParseFromImport();

        Node *ParseIdentifier();

        Node *ParseIDValue(NodeType type, const scanner::Position &start);

        Node *ParseIF();

        Node *ParseImport();

        Node *ParseIn(Node *left);

        Node *ParseInfix(Node *left);

        Node *ParseInit(Node *left);

        Node *ParseList();

        Node *ParseLiteral();

        Node *ParseLoop();

        Node *ParseOOBCall();

        Node *ParsePipeline(Node *left);

        Node *ParsePostInc(Node *left);

        Node *ParsePrefix();

        Node *ParseScope();

        Node *ParseSelector(Node *left);

        Node *ParseStatement(ParserScope scope);

        Node *ParseStructDecl();

        Node *ParseSubscript(Node *left);

        Node *ParseSwitch();

        Node *ParseSwitchCase();

        Node *ParseTernary(Node *left);

        Node *ParseTraitDecl();

        Node *ParseVarDecl(bool visibility, bool constant, bool weak);

        Node *ParseVarDeclTuple(const scanner::Token &token, bool visibility, bool constant, bool weak);

        Node *ParseUnaryStmt(NodeType type, bool expr_required);

        [[nodiscard]] NudMeth LookupNud(lang::scanner::TokenType token) const;

        void Eat();

        void IgnoreNL();

    public:
        Parser(const char *filename, lang::scanner::Scanner &scanner) noexcept: scanner_(scanner),
                                                                                filename_(filename) {}

        File *Parse() noexcept;
    };

} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSER_H_
