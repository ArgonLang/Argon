// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_COMPILER_H_
#define ARGON_LANG_COMPILER_COMPILER_H_

#include <object/datatype/code.h>
#include <object/datatype/map.h>
#include <object/datatype/namespace.h>

#include <lang/parser/nodes.h>
#include <lang/opcodes.h>

#include "symtable.h"
#include "translation_unit.h"

namespace argon::lang::compiler {
    class Compiler {
        TranslationUnit *unit_ = nullptr;

        SymbolTable *symt = nullptr;

        argon::object::Map *statics_globals_ = nullptr;

        bool Compile_(argon::lang::parser::Node *node);

        bool CompileExpression(argon::lang::parser::Node *expr);

        bool CompileBinary(argon::lang::parser::Binary *expr);

        bool CompileUnary(argon::lang::parser::Unary *expr);

        bool Emit(argon::lang::OpCodes op, int arg, BasicBlock *dest);

        bool Emit(argon::lang::OpCodes op, unsigned char flags, unsigned short arg);

        bool IdentifierLoad(argon::object::String *name);

        bool IdentifierNew(argon::object::String *name, SymbolType stype, object::PropertyType ptype, bool emit);

        bool TScopeNew(argon::object::String *name, TUScope scope);

        int PushStatic(object::ArObject *obj, bool store, bool emit);

        Symbol *IdentifierLookupOrCreate(argon::object::String *name, SymbolType type);

        void TScopeExit();

        void TScopeClear();

    public:
        Compiler() noexcept = default;

        [[nodiscard]] argon::object::Code *Compile(argon::lang::parser::File *node);
    };
}

#endif // !ARGON_LANG_COMPILER_COMPILER_H_
