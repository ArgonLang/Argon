// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_PARSER_H_
#define ARGON_LANG_PARSER_PARSER_H_

#include <lang/scanner/scanner.h>

#include "docstring.h"
#include "node.h"
#include "parsererr.h"

namespace argon::lang::parser {
    constexpr const char *kParserError[] = {
            (const char *) "ParserError"
    };

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

        const char *filename_ = nullptr;

        DocString *doc_string_ = nullptr;

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

        void IgnoreNewLineIF(scanner::TokenType type) {
            const scanner::Token *peek;

            if (!this->scanner_.PeekToken(&peek))
                throw ScannerException();

            if(peek->type == type)
                this->Eat();
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

            this->Eat();
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

        Node *ParseAwait();

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

        Node *ParseFromImport(bool pub);

        Node *ParseIdentifier();

        Node *ParseIDValue(NodeType type, const scanner::Position &start);

        Node *ParseIF();

        Node *ParseImport(bool pub);

        Node *ParseIn(Node *left);

        Node *ParseInfix(Node *left);

        Node *ParseInit(Node *left);

        Node *ParseList();

        Node *ParseLiteral();

        Node *ParseLoop();

        Node *ParseNullCoalescing(Node *left);

        Node *ParseOOBCall();

        Node *ParsePipeline(Node *left);

        Node *ParsePostInc(Node *left);

        Node *ParsePrefix();

        Node *ParseScope();

        Node *ParseSelector(Node *left);

        Node *ParseStatement(ParserScope scope);

        Node *ParseStructDecl(bool pub);

        Node *ParseSubscript(Node *left);

        Node *ParseSwitch();

        Node *ParseSwitchCase();

        Node *ParseTernary(Node *left);

        Node *ParseTraitDecl(bool pub);

        Node *ParseVarDecl(bool visibility, bool constant, bool weak);

        Node *ParseVarDeclTuple(const scanner::Token &token, bool visibility, bool constant, bool weak);

        Node *ParseUnaryStmt(NodeType type, bool expr_required);

        [[nodiscard]] NudMeth LookupNud(lang::scanner::TokenType token) const;

        void Eat();

        void EnterDocContext();

        void ExitDocContext() {
            this->doc_string_ = DocStringDel(this->doc_string_);
        }

        void IgnoreNL();

    public:
        /**
         * @brief Initialize the parser with a filename and the scanner.
         *
         * @param filename Source code name.
         * @param scanner Reference to Scanner.
         */
        Parser(const char *filename, lang::scanner::Scanner &scanner) noexcept: scanner_(scanner),
                                                                                filename_(filename) {}

        ~Parser() {
            while (this->doc_string_ != nullptr)
                this->doc_string_ = DocStringDel(this->doc_string_);
        }

        /**
         * @brief Parses the source code.
         *
         * @return A pointer to the first node of the AST or nullptr in case of error.
         */
        File *Parse() noexcept;
    };

} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_PARSER_H_
