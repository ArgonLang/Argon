// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_H_
#define ARGON_LANG_PARSER_H_

#include <lang/scanner/scanner.h>
#include <lang/ast/ast.h>

#include <list>

namespace lang {
    class Parser {
        std::unique_ptr<scanner::Scanner> scanner_;
        scanner::Token currTk_;

        void Eat();

        void Eat(scanner::TokenType type, std::string errmsg);

        void EatTerm(bool must_eat);

        void EatTerm(bool must_eat, scanner::TokenType stop_token);

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

        ast::NodeUptr TraitList();

        ast::NodeUptr ImplDecl();

        // *** STATEMENTS ***

        ast::NodeUptr Statement();

        ast::NodeUptr ImportStmt();

        ast::NodeUptr FromImportStmt();

        ast::NodeUptr ImportAsName();

        ast::NodeUptr DottedAsName();

        std::string DottedName();

        ast::NodeUptr ForStmt();

        ast::NodeUptr LoopStmt();

        ast::NodeUptr IfStmt(bool eatIf);

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

        ast::NodeUptr UnaryExpr();

        ast::NodeUptr AtomExpr();

        ast::NodeUptr ParseArguments(ast::NodeUptr left);

        ast::NodeUptr ParseSubscript();

        ast::NodeUptr MemberAccess(ast::NodeUptr left);

        ast::NodeUptr ParseAtom();

        ast::NodeUptr ParseArrowOrTuple();

        std::list<ast::NodeUptr> ParsePeap();

        ast::NodeUptr ParseList();

        ast::NodeUptr ParseMapOrSet();

        void ParseMap(std::unique_ptr<ast::List> &map);

        ast::NodeUptr ParseScope();

        bool TokenInRange(scanner::TokenType begin, scanner::TokenType end) {
            return this->currTk_.type > begin && this->currTk_.type < end;
        }

        bool Match(scanner::TokenType type) {
            return this->currTk_.type == type;
        }

        template<typename ...TokenTypes>
        bool Match(scanner::TokenType type, TokenTypes... types) {
            if (!this->Match(type))
                return this->Match(types...);
            return true;
        }

    public:
        Parser(std::istream *src) : Parser("", src) {}

        Parser(std::string filename, std::istream *source);

        std::unique_ptr<ast::Program> Parse();
    };
}  // namespace lang

#endif // !ARGON_LANG_PARSER_H_
