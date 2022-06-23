// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_PARSER_H_
#define ARGON_LANG_PARSER_PARSER_H_

#include <utils/enum_bitmask.h>

#include <lang/scanner/scanner.h>

#include "nodes.h"

namespace argon::lang::parser {
    enum class DisableFeature {
        DECL_VAR = 0x01,
        DECL_LET = 0x01 << 1u,
        DECL_STRUCT = 0x01 << 2u,
        DECL_TRAIT = 0x01 << 3u,
        MODIFIER_PUB = 0x01 << 4u,
        MODIFIER_WEAK = 0x01 << 5u,
        INIT = 0x01 << 6u
    };
}

ENUMBITMASK_ENABLE(argon::lang::parser::DisableFeature);

namespace argon::lang::parser {
    constexpr auto disable_static = (DisableFeature) 0x3E;

    class Parser;

    using NudMeth = Node *(Parser::*)();
    using LedMeth = Node *(Parser::*)(Node *left);

    class Parser {
        scanner::Scanner *scanner_;
        const char *filename_;

        scanner::Token tkcur_;

        DisableFeature dflag_;

        bool IsLiteral();

        [[nodiscard]] bool Match(scanner::TokenType type) const {
            return this->tkcur_.type == type;
        }

        template<typename ...TokenTypes>
        [[nodiscard]] bool Match(scanner::TokenType type, TokenTypes... types) const {
            if (!this->Match(type))
                return this->Match(types...);
            return true;
        }

        bool MatchEat(scanner::TokenType type, bool eat_nl);

        [[nodiscard]] bool TokenInRange(scanner::TokenType begin, scanner::TokenType end) const;

        [[nodiscard]] bool ParseRestElement(Node **rest_node);

        [[nodiscard]] LedMeth LookupLed() const;

        [[nodiscard]] argon::object::ArObject *
        ParseArgsKwargs(const char *partial_error, const char *error, bool *is_kwargs);

        [[nodiscard]] object::ArObject *ScopeAsNameList(bool id_only, bool with_alias);

        [[nodiscard]] argon::object::ArObject *TraitList();

        [[nodiscard]] Node *ParseAssertion();

        [[nodiscard]] Node *ParseAssignment(Node *left);

        [[nodiscard]] Node *ParseOOBCall();

        [[nodiscard]] Node *ParseBlock(DisableFeature dflag);

        [[nodiscard]] Node *TypeDeclBlock(bool is_struct);

        [[nodiscard]] Node *ParseScope();

        [[nodiscard]] Node *Expression();

        [[nodiscard]] Node *ParseMultiDecl(const scanner::Token &token);

        [[nodiscard]] Node *ParseVarDecl(bool constant, bool pub);

        [[nodiscard]] Node *ParseElvis(Node *left);

        [[nodiscard]] Node *ParseExpr(int precedence);

        [[nodiscard]] inline Node *ParseExpr() { return this->ParseExpr(0); }

        [[nodiscard]] Node *ParseFnCall(Node *left);

        [[nodiscard]] Node *ParseFromImport();

        [[nodiscard]] Node *ParseFor();

        [[nodiscard]] Node *FuncDecl(bool pub, bool nobody);

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

        [[nodiscard]] Node *ParsePanic();

        [[nodiscard]] Node *ParsePipeline(Node *left);

        [[nodiscard]] Node *ParsePostUpdate(Node *left);

        [[nodiscard]] Node *ParsePreUpdate();

        [[nodiscard]] Node *ParsePrefix();

        [[nodiscard]] Node *ParseReturn();

        [[nodiscard]] Node *ParseYield();

        [[nodiscard]] Node *ParseStatement();

        [[nodiscard]] Node *SwitchCase();

        [[nodiscard]] Node *SwitchDecl();

        [[nodiscard]] Node *StructDecl(bool pub);

        [[nodiscard]] Node *ParseSubscript(Node *left);

        [[nodiscard]] Node *ParseTernary(Node *left);

        [[nodiscard]] Node *ParseExprList(Node *left);

        [[nodiscard]] Node *TraitDecl(bool pub);

        [[nodiscard]] Node *ParseTupleLambda();

        [[nodiscard]] Node *ScopeAsName(bool id_only, bool with_alias);

        [[nodiscard]] Node *ParseDecls(DisableFeature dflag);

        [[nodiscard]] NudMeth LookupNud();

        void Eat();

        void EatTerm(scanner::TokenType stop);

        void EatTerm() { this->EatTerm(scanner::TokenType::END_OF_FILE); }

    public:
        Parser(scanner::Scanner *scanner, const char *filename);

        File *Parse();
    };

}  // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSER_H_
