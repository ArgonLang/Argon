// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <vm/datatype/code.h>
#include <vm/datatype/dict.h>

#include <lang/parser/node.h>

#include "translation_unit.h"

namespace argon::lang {
    class Compiler {
        TranslationUnit *unit_ = nullptr;

        vm::datatype::Dict *statics_globals_ = nullptr;

        int LoadStatic(vm::datatype::ArObject *value, bool store, bool emit);

        SymbolT *IdentifierLookupOrCreate(vm::datatype::String *name, SymbolType type);

        void Binary(const parser::Binary *binary);

        void Compile(const parser::Node *node);

        void CompileElvis(const parser::Test *test);

        void CompileInit(const parser::Initialization *init);

        void CompileLTDS(const parser::Unary *list);

        void CompileSafe(const parser::Unary *unary);

        void CompileSelector(const parser::Binary *selector, bool dup, bool emit);

        void CompileSubscr(const parser::Subscript *subscr, bool dup, bool emit);

        void CompileTernary(const parser::Test *test);

        void CompileTest(const parser::Binary *test);

        void CompileUnary(const parser::Unary *unary);

        void CompileUpdate(const parser::Unary *update);

        void Expression(const parser::Node *node);

        void LoadIdentifier(vm::datatype::String *identifier);

        void StoreVariable(vm::datatype::String *name);

        void TUScopeEnter(vm::datatype::String *name, SymbolType context);

        void TUScopeExit();

    public:
        Compiler() noexcept = default;

        ~Compiler();

        [[nodiscard]] argon::vm::datatype::Code *Compile(argon::lang::parser::File *node);
    };
}

#endif // !ARGON_LANG_COMPILER_H_
