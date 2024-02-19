// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_PARSER2_H_
#define ARGON_LANG_PARSER2_PARSER2_H_

#include <argon/lang/scanner/scanner.h>

#include <argon/lang/parser2/node/node.h>

#include <argon/lang/exception.h>
#include <argon/lang/parser2/context.h>

namespace argon::lang::parser2 {
    constexpr const char *kStandardError[] = {
            "invalid syntax",
            "'var' keyword cannot be used within a trait",
            "'weak' keyword is only allowed within the context of a struct",
            "after 'weak' the 'var' keyword is required",
            "expected identifier after '%s'",
            "expected '=' after identifier(s) in let declaration",
            "expected ')' after function params",
            "expected '{' to start a code block",
            "expected identifier before '=' in named parameter declaration",
            "expected identifier",
            "only one &-param is allowed per function declaration",
            "only one rest-param is allowed per function declaration",
            "unexpected [named] param",
            "'%s' not supported in '%s' context",
            "sync block requires an object reference, not a literal",
            "expected import path as string after '%s'",
            "expected 'import' after module path",
            "expected module name or '*'",
            "expected declaration after 'pub' keyword",
            "expected statement after label",
            "expected ']' after %s definition",
            "you started defining a set, not a dict",
            "you started defining a dict, not a set",
            "expected '}' after %s definition",
            "expected ')' after tuple/function definition",
            "subscript definition (index | slice) cannot be empty",
            "unexpected update operator",
            "expected identifier after '%s' operator",
            "expression on the left cannot be used as a target for the assignment expression",
            "expected identifiers before '%s'",
            "can't mix field names with positional initialization",
            "expected ')' after struct initialization",
            "expected ')' after last argument of function call",
            "function parameters must be passed in the order: [positional][, named param][, spread][, kwargs]",
            "only identifiers are allowed before the '=' sign",
            "unexpected label after fallthrough",
            "%s expected call expression",
            "expected var declaration, identifier or tuple before 'of' in foreach",
            "unexpected initialization of var in foreach",
            "expected ';' after for initialization",
            "expected ';' after test",
            "expected '{' after switch declaration",
            "default case already defined",
            "expected 'case' or 'default' label",
            "expected ':' after '%s' label",
            "expected for, foreach or loop after label"
    };

    class Parser {
        using LedMeth = node::Node *(Parser::*)(Context *context, node::Node *);
        using NudMeth = node::Node *(Parser::*)(Context *context);

        scanner::Token tkcur_;

        scanner::Scanner &scanner_;

        const char *filename_;

        [[nodiscard]] bool CheckIDExt() const;

        [[nodiscard]] static bool CheckScope(Context *context, ContextType type) {
            return context->type == type;
        }

        template<typename ...ContextTypes>
        [[nodiscard]] static bool CheckScope(Context *context, ContextType type, ContextTypes... types) {
            if (Parser::CheckScope(context, type))
                return true;

            return Parser::CheckScope(context, types...);
        }

        [[nodiscard]] bool Match(scanner::TokenType type) const {
            return this->tkcur_.type == type;
        }

        template<typename ...TokenTypes>
        [[nodiscard]] bool Match(scanner::TokenType type, TokenTypes... types) const {
            if (!this->Match(type))
                return this->Match(types...);
            return true;
        }

        bool MatchEat(scanner::TokenType type, bool ignore_nl) {
            if (ignore_nl && this->tkcur_.type == scanner::TokenType::END_OF_LINE)
                this->Eat(true);

            if (this->Match(type)) {
                this->Eat(ignore_nl);
                return true;
            }

            return false;
        }

        [[nodiscard]] bool TokenInRange(scanner::TokenType begin, scanner::TokenType end) const {
            return this->tkcur_.type > begin && this->tkcur_.type < end;
        }

        static int PeekPrecedence(scanner::TokenType type);

        List *ParseFnParams(Context *context, bool parse_pexpr);

        List *ParseTraitList();

        node::Node *ParseAsync(Context *context, const scanner::Position &start, bool pub);

        node::Node *ParseAssertion(Context *context);

        node::Node *ParseBCFStatement(Context *context);

        node::Node *ParseBlock(Context *context);

        node::Node *ParseDecls(Context *context);

        node::Node *ParseExpression(Context *context);

        node::Node *ParseForLoop(Context *context);

        node::Node *ParseFromImport(bool pub);

        node::Node *ParseFunc(Context *context, const scanner::Position &start, bool pub);

        node::Node *ParseIf(Context *context);

        node::Node *ParseImport(bool pub);

        node::Node *ParseLiteral(Context *context);

        node::Node *ParseLoop(Context *context);

        node::Node *ParseOOBCall(Context *context);

        node::Node *ParsePRYStatement(Context *context, node::NodeType type);

        node::Node *ParseFuncNameParam(Context *context, bool parse_pexpr);

        node::Node *ParseFuncParam(const scanner::Position &start, node::NodeType type);

        node::Node *ParseScope();

        node::Node *ParseStatement(Context *context);

        node::Node *ParseStruct(Context *context, bool pub);

        node::Node *ParseSyncBlock(Context *context);

        node::Node *ParseSwitch(Context *context);

        node::Node *ParseSwitchCase(Context *context);

        node::Node *ParseTrait(Context *context, bool pub);

        node::Node *ParseVarDecl(Context *context, const scanner::Position &start, bool constant, bool pub, bool weak);

        node::Node *ParseVarDecls(const scanner::Token &token, node::Assignment *vardecl);

        String *ParseDoc();

        static node::Unary *AssignmentIDs2Tuple(const node::Assignment *assignment);

        static node::Unary *String2Identifier(const scanner::Position &start, const scanner::Position &end, String *value);

        static String *ParseIdentifierSimple(const scanner::Token *token);

        void Eat(bool ignore_nl);

        void EatNL();

        void IgnoreNewLineIF(scanner::TokenType type) {
            const scanner::Token *peek;

            if (this->tkcur_.type != scanner::TokenType::END_OF_LINE)
                return;

            if (!this->scanner_.PeekToken(&peek))
                throw ScannerException();

            if (peek->type == type)
                this->Eat(true);
        }

        template<typename ...TokenTypes>
        void IgnoreNewLineIF(scanner::TokenType type, TokenTypes... types) {
            const scanner::Token *peek;

            if (this->tkcur_.type != scanner::TokenType::END_OF_LINE)
                return;

            if (!this->scanner_.PeekToken(&peek))
                throw ScannerException();

            if (peek->type != type) {
                this->IgnoreNewLineIF(types...);
                return;
            }

            this->Eat(true);
        }

// *********************************************************************************************************************
// EXPRESSION-ZONE AFTER THIS POINT
// *********************************************************************************************************************

        bool ParseFuncCallNamedArg(Context *context, ARC &k_args, node::Node *node, bool must_parse);

        bool ParseFuncCallSpread(List *args, node::Node *node, bool must_parse);

        bool ParseFuncCallUnpack(ARC &k_args, bool must_parse);

        static LedMeth LookupLED(scanner::TokenType token, bool newline);

        node::Node *ParseArrowOrTuple(Context *context);

        node::Node *ParseAssignment(Context *context, node::Node *left);

        node::Node *ParseAwait(Context *context);

        node::Node *ParseChanOut(Context *context);

        node::Node *ParseDictSet(Context *context);

        node::Node *ParseElvis(Context *context, node::Node *left);

        node::Node *ParseExpression(Context *context, int precedence);

        node::Node *ParseExpressionList(Context *context, node::Node *left);

        node::Node *ParseFuncCall(Context *context, node::Node *left);

        node::Node *ParseIdentifier(Context *context);

        node::Node *ParseIn(Context *context, node::Node *left);

        node::Node *ParseInfix(Context *context, node::Node *left);

        node::Node *ParseInit(Context *context, node::Node *left);

        node::Node *ParseList(Context *context);

        node::Node *ParseNullCoalescing(Context *context, node::Node *left);

        node::Node *ParsePipeline(Context *context, node::Node *left);

        node::Node *ParsePostInc(Context *context, node::Node *left);

        node::Node *ParsePrefix(Context *context);

        node::Node *ParseSelector(Context *context, node::Node *left);

        node::Node *ParseSubscript(Context *context, node::Node *left);

        node::Node *ParseTernary(Context *context, node::Node *left);

        node::Node *ParseTrap(Context *context);

        node::Node *ParseWalrus(Context *context, node::Node *left);

        static NudMeth LookupNUD(scanner::TokenType token);

    public:
        /**
         * @brief Initialize the parser with a filename and the scanner.
         *
         * @param filename Source code name.
         * @param scanner Reference to Scanner.
         */
        Parser(const char *filename, scanner::Scanner &scanner) noexcept: scanner_(scanner),
                                                                          filename_(filename) {}

        /**
         * @brief Parses the source code.
         *
         * @return A pointer to the first node of the AST or nullptr in case of error.
         */
        node::Module *Parse();
    };
} // namespace argon::lang::parser2

#endif // ARGON_LANG_PARSER2_PARSER2_H_
