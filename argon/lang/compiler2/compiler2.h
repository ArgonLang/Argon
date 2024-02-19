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
            "invalid AST node, expected '%s', got: '%s'"
    };

    class Compiler {
        argon::vm::datatype::Dict *static_globals_;

        TranslationUnit *unit_;

        int LoadStatic(const parser2::node::Unary *literal, bool store, bool emit);

        void Compile(const parser2::node::Node *node);

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

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
