// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_H_
#define ARGON_LANG_PARSER_H_

#include <lang/scanner/scanner.h>
#include <lang/ast/ast.h>

#include <list>

namespace argon::lang {
    class Parser {
        std::unique_ptr<scanner::Scanner> scanner_;
        std::list<ast::Comment> comments_;
        scanner::Token currTk_;
        std::string filename_;

        unsigned short safe_ = 0;

        std::list<ast::Comment>::iterator BeginDocs();

        std::list<ast::Comment> GetDocs(std::list<ast::Comment>::iterator &pos);

        // *** DECLARATIONS ***

        ast::NodeUptr Declaration();

        ast::NodeUptr AccessModifier();

        ast::NodeUptr SmallDecl(bool pub);

        ast::NodeUptr AliasDecl(bool pub);

        ast::NodeUptr VarModifier(bool pub);

        ast::NodeUptr VarDecl(bool pub);

        ast::NodeUptr ConstDecl(bool pub);

        ast::NodeUptr FuncDecl(bool pub);

        std::list<lang::ast::NodeUptr> Param();

        ast::NodeUptr Variadic();

        ast::NodeUptr StructDecl(bool pub);

        ast::NodeUptr StructBlock();

        ast::NodeUptr TraitDecl(bool pub);

        ast::NodeUptr TraitBlock();

        std::list<ast::NodeUptr> TraitList();

        ast::NodeUptr ImplDecl();

        // *** STATEMENTS ***

        ast::NodeUptr Statement();

        ast::NodeUptr RtnStmt();

        ast::NodeUptr ImportStmt();

        ast::NodeUptr FromImportStmt();

        ast::NodeUptr ScopeAsName(bool id_only, bool with_alias);

        ast::NodeUptr ForStmt();

        ast::NodeUptr LoopStmt();

        ast::NodeUptr IfStmt();

        ast::NodeUptr SwitchStmt();

        ast::NodeUptr SwitchCase();

        ast::NodeUptr JmpStmt();

        ast::NodeUptr Block();

        // *** EXPRESSIONS ***

        ast::NodeUptr Expression();

        ast::NodeUptr TestList();

        ast::NodeUptr Test();

        ast::NodeUptr OrTest();

        ast::NodeUptr AndTest();

        ast::NodeUptr OrExpr();

        ast::NodeUptr XorExpr();

        ast::NodeUptr AndExpr();

        ast::NodeUptr EqualityExpr();

        ast::NodeUptr RelationalExpr();

        ast::NodeUptr ShiftExpr();

        ast::NodeUptr ArithExpr();

        ast::NodeUptr MulExpr();

        ast::NodeUptr UnaryExpr(bool first);

        ast::NodeUptr AtomExpr();

        ast::NodeUptr Initializer(ast::NodeUptr left);

        ast::NodeUptr ParseArguments(ast::NodeUptr left);

        ast::NodeUptr ParseSubscript();

        ast::NodeUptr MemberAccess(ast::NodeUptr left);

        ast::NodeUptr ParseAtom();

        ast::NodeUptr ParseArrowOrTuple();

        ast::NodeUptr ParseList();

        ast::NodeUptr ParseMapOrSet();

        ast::NodeUptr ParseScope();

        bool Match(scanner::TokenType type) {
            return this->currTk_.type == type;
        }

        template<typename ...TokenTypes>
        bool Match(scanner::TokenType type, TokenTypes... types) {
            if (!this->Match(type))
                return this->Match(types...);
            return true;
        }

        bool MatchEat(scanner::TokenType type, bool eat_nl);

        bool MatchEatNL(scanner::TokenType type);

        bool TokenInRange(scanner::TokenType begin, scanner::TokenType end);

        void Eat();

        void Eat(scanner::TokenType type, std::string errmsg);

        void EatTerm(bool must_eat);

        void EatTerm(bool must_eat, scanner::TokenType stop_token);

    public:
        explicit Parser(std::istream *src) : Parser("", src) {}

        Parser(std::string filename, std::istream *source);

        std::unique_ptr<ast::Program> Parse();
    };
}  // namespace lang

#endif // !ARGON_LANG_PARSER_H_
