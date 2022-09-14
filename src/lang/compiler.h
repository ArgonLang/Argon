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

        void Binary(const parser::Binary *binary);

        void Compile(const parser::Node *node);

        void CompileLTDS(const parser::Unary *list);

        void Expression(const parser::Node *node);

        void TUScopeEnter(vm::datatype::String *name, SymbolType context);

        void TUScopeExit();

    public:
        Compiler() noexcept = default;

        ~Compiler();

        [[nodiscard]] argon::vm::datatype::Code *Compile(argon::lang::parser::File *node);
    };
}

#endif // !ARGON_LANG_COMPILER_H_
