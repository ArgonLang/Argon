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

        [[nodiscard]] object::ArObject *ScopeAsNameList(bool id_only, bool with_alias);

        [[nodiscard]] argon::object::ArObject *TraitList();

        [[nodiscard]] Node *ParseOOBCall();

        [[nodiscard]] Node *ParseBlock();

        [[nodiscard]] Node *TypeDeclBlock(bool is_struct);

        [[nodiscard]] Node *ParseScope();

        [[nodiscard]] Node *Expression();

        [[nodiscard]] Node *ParseVarDecl(bool constant, bool pub);

        [[nodiscard]] Node *ParseElvis(Node *left);

        [[nodiscard]] Node *ParseExpr(int precedence);

        [[nodiscard]] inline Node *ParseExpr() { return this->ParseExpr(0); }

        [[nodiscard]] Node *ParseFnCall(Node *left);

        [[nodiscard]] Node *ParseFromImport();

        [[nodiscard]] Node *ParseFor();

        [[nodiscard]] Node *FuncDecl(bool pub);

        [[nodiscard]] Node *ParseIdentifier();

        [[nodiscard]] Node *ParseJmpStmt();

        [[nodiscard]] Node *ParseList();

        [[nodiscard]] Node *ParseLiteral();

        [[nodiscard]] Node *ParseLoop();

        [[nodiscard]] Node *ParseIf();

        [[nodiscard]] Node *ParseInfix(Node *left);

        [[nodiscard]] Node *ParseInitialization(Node *left);

        [[nodiscard]] Node *ParseImport();

        [[nodiscard]] Node *ParseMapSet();

        [[nodiscard]] Node *ParseMemberAccess(Node *left);

        [[nodiscard]] Node *ParsePostUpdate(Node *left);

        [[nodiscard]] Node *ParsePreUpdate();

        [[nodiscard]] Node *ParsePrefix();

        [[nodiscard]] Node *ParseReturn();

        [[nodiscard]] Node *ParseDecls();

        [[nodiscard]] Node *ParseStatement();

        [[nodiscard]] Node *SwitchCase();

        [[nodiscard]] Node *SwitchDecl();

        [[nodiscard]] Node *StructDecl(bool pub);

        [[nodiscard]] Node *ParseSubscript(Node *left);

        [[nodiscard]] Node *ParseTernary(Node *left);

        [[nodiscard]] Node *ParseTestList();

        [[nodiscard]] Node *TraitDecl(bool pub);

        [[nodiscard]] Node *ParseTupleLambda();

        [[nodiscard]] Node *ScopeAsName(bool id_only, bool with_alias);

        [[nodiscard]] Node *SmallDecl(bool pub);

        [[nodiscard]] NudMeth LookupNud();

        void Eat();

        void EatTerm();

    public:
        Parser(scanner2::Scanner *scanner, const char *filename);

        File *Parse();
    };

}  // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSER_H_
