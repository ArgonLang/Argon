// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_PARSER2_H_
#define ARGON_LANG_PARSER2_PARSER2_H_

#include <argon/lang/scanner/scanner.h>

#include <argon/lang/parser2/node/node.h>

#include <argon/lang/parser2/context.h>

namespace argon::lang::parser2 {
    class Parser {
        scanner::Token tkcur_;

        scanner::Scanner &scanner_;

        Context *context_{};

        const char *filename_;

        [[nodiscard]] bool CheckScope(Context *context, ContextType type) {
            return context->type == type;
        }

        template<typename ...ContextTypes>
        [[nodiscard]] bool CheckScope(Context *context, ContextType type, ContextTypes... types) {
            if (this->CheckScope(context, type))
                return true;

            return this->CheckScope(context, types...);
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
            if (this->Match(type)) {
                this->Eat(ignore_nl);
                return true;
            }

            return false;
        }

        [[nodiscard]] bool TokenInRange(scanner::TokenType begin, scanner::TokenType end) const {
            return this->tkcur_.type > begin && this->tkcur_.type < end;
        }

        node::Node *ParseDecls(Context *context);

        void Eat(bool ignore_nl);

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
