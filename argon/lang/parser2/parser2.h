// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_PARSER2_H_
#define ARGON_LANG_PARSER2_PARSER2_H_

#include <argon/lang/scanner/scanner.h>

#include <argon/lang/parser2/node/node.h>

#include <argon/lang/parser2/exception.h>
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
            "sync block requires an object reference, not a literal"
    };

    class Parser {
        scanner::Token tkcur_;

        scanner::Scanner &scanner_;

        const char *filename_;

        [[nodiscard]] bool CheckIDExt();

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

        List *ParseFnParams();

        node::Node *ParseAsync(Context *context, scanner::Position &start, bool pub);

        node::Node *ParseBlock(Context *context);

        node::Node *ParseDecls(Context *context);

        node::Node *ParseFunc(Context *context, scanner::Position start, bool pub);

        node::Node *ParseFuncNameParam(bool parse_pexpr);

        node::Node *ParseFuncParam(scanner::Position start, node::NodeType type);

        static node::Node *ParseIdentifier(scanner::Token *token);

        node::Node *ParseSyncBlock(Context *context);

        node::Node *ParseVarDecl(Context *context, scanner::Position start, bool constant, bool pub, bool weak);

        node::Node *ParseVarDecls(const scanner::Token &token);

        String *ParseDoc();

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
