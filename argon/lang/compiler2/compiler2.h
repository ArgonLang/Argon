// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_COMPILER2_H_
#define ARGON_LANG_COMPILER2_COMPILER2_H_

#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/dict.h>

#include <argon/lang/compiler2/transl_unit.h>
#include <argon/lang/compiler2/symt.h>

#include <argon/lang/parser2/parser2.h>

namespace argon::lang::compiler2 {
    constexpr const char *kCompilerErrors[] = {
            "invalid AST node, expected '%s', got: '%s'",
            "invalid NodeType(%d) for %s",
            "invalid TokenType(%d) for %s",
            "cannot use '%s' as identifier"
    };

    class Compiler {
        argon::vm::datatype::Dict *static_globals_ = nullptr;

        TranslationUnit *unit_ = nullptr;

        SymbolT *IdentifierLookupOrCreate(String *id, SymbolType type);

        void Compile(const parser2::node::Node *node);

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

        int LoadStatic(ArObject *object, const scanner::Loc *loc, bool store, bool emit);

        int LoadStatic(const parser2::node::Unary *literal, bool store, bool emit);

        int LoadStaticNil(const scanner::Loc *loc, bool emit);

        void LoadIdentifier(String *identifier, const scanner::Loc *loc);

        void LoadIdentifier(const parser2::node::Unary *identifier);

        void CompileDLST(const parser2::node::Unary *unary);

        void CompileElvis(const parser2::node::Binary *binary);

        void CompileInfix(const parser2::node::Binary *binary);

        void CompileNullCoalescing(const parser2::node::Binary *binary);

        void CompilePrefix(const parser2::node::Unary *unary);

        void CompileSubscr(const parser2::node::Subscript *subscr, bool dup, bool emit);

        void CompileTest(const parser2::node::Binary *binary);

        void CompileTernary(const parser2::node::Branch *branch);

        void Expression(const parser2::node::Node *node);

// *********************************************************************************************************************
// PRIVATE
// *********************************************************************************************************************

        void EnterScope(argon::vm::datatype::String *name, SymbolType type);

        void ExitScope();

    public:
        Compiler() noexcept = default;

        [[nodiscard]] argon::vm::datatype::Code *Compile(argon::lang::parser2::node::Module *mod);
    };
} // argon::lang::compiler2

#endif // ARGON_LANG_COMPILER2_COMPILER2_H_
