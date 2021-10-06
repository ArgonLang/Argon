// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_COMPILER_H_
#define ARGON_LANG_COMPILER_COMPILER_H_

#include <object/datatype/code.h>
#include <object/datatype/map.h>

#include <lang/parser/nodes.h>
#include <lang/opcodes.h>

#include "translation_unit.h"

namespace argon::lang::compiler {
    class Compiler {
        TranslationUnit *unit_ = nullptr;

        argon::object::Map *statics_globals_ = nullptr;

        bool Compile_(argon::lang::parser::Node *node);

        bool CompileExpression(argon::lang::parser::Node *expr);

        bool CompileBinary(argon::lang::parser::Binary *expr);

        bool CompileUnary(argon::lang::parser::Unary *expr);

        bool Emit(argon::lang::OpCodes op, int arg);

        bool TScopeNew(argon::object::String *name, TUScope scope);

        int PushStatic(object::ArObject *obj, bool store, bool emit);

        void TScopeExit();

        void TScopeClear();

    public:
        Compiler() noexcept = default;

        [[nodiscard]] argon::object::Code *Compile(argon::lang::parser::File *node);
    };
}

#endif // !ARGON_LANG_COMPILER_COMPILER_H_
