// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <vm/datatype/code.h>
#include <vm/datatype/dict.h>
#include <vm/datatype/function.h>
#include <vm/datatype/namespace.h>

#include <lang/parser/node.h>

#include "translation_unit.h"

namespace argon::lang {
    class Compiler {
        TranslationUnit *unit_ = nullptr;

        vm::datatype::Dict *statics_globals_ = nullptr;

        int LoadStatic(vm::datatype::ArObject *value, bool store, bool emit);

        vm::datatype::String *MakeFname();

        SymbolT *IdentifierLookupOrCreate(vm::datatype::String *name, SymbolType type);

        void Binary(const parser::Binary *binary);

        void Compile(const parser::Node *node);

        void CompileBlock(const parser::Node *node, bool sub);

        void CompileElvis(const parser::Test *test);

        void CompileFunction(const parser::Function *func);

        void CompileFunctionDefaultBody();

        void CompileFunctionParams(vm::datatype::List *params, unsigned short &p_count,
                                   vm::datatype::FunctionFlags &flags);

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

        void IdentifierNew(vm::datatype::String *name, SymbolType stype, vm::datatype::AttributeFlag aflags, bool emit);

        void IdentifierNew(const char *name, SymbolType stype, vm::datatype::AttributeFlag aflags, bool emit);

        void LoadIdentifier(vm::datatype::String *identifier);

        void PushAtom(const char *key, bool emit);

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
