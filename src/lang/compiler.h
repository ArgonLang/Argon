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
    constexpr const char *kCompilerError[] = {
            (const char *) "CompilerError"
    };

    class Compiler {
        TranslationUnit *unit_ = nullptr;

        vm::datatype::Dict *statics_globals_ = nullptr;

        int CompileSelector(const parser::Binary *selector, bool dup, bool emit);

        int LoadStatic(vm::datatype::ArObject *value, bool store, bool emit);

        vm::datatype::String *MakeFname();

        static vm::datatype::String *MakeImportName(const vm::datatype::String *mod_name);

        vm::datatype::String *MakeQname(vm::datatype::String *name);

        SymbolT *IdentifierLookupOrCreate(vm::datatype::String *name, SymbolType type);

        void Binary(const parser::Binary *binary);

        void Compile(const parser::Node *node);

        void CompileAssertion(const parser::Binary *binary);

        void CompileAssignment(const parser::Binary *assign);

        void CompileAugAssignment(const parser::Binary *assign);

        void CompileBlock(const parser::Node *node, bool sub);

        void CompileCall(const parser::Call *call);

        void CompileCallKwArgs(vm::datatype::ArObject *args, int &args_count, vm::OpCodeCallMode &mode);

        void CompileCallPositional(vm::datatype::ArObject *args, int &args_count, vm::OpCodeCallMode &mode);

        void CompileConstruct(const parser::Construct *construct);

        void CompileContains(const parser::Binary *contains);

        void CompileDeclaration(const parser::Assignment *declaration);

        void CompileElvis(const parser::Binary *binary);

        void CompileForEach(const parser::Loop *loop);

        void CompileForLoop(const parser::Loop *loop);

        void CompileForLoopInc(const parser::Node *node);

        void CompileFunction(const parser::Function *func);

        void CompileFunctionDefaultBody(const parser::Function *func, vm::datatype::String *fname);

        void CompileFunctionParams(vm::datatype::List *params, unsigned short &p_count,
                                   vm::datatype::FunctionFlags &flags);

        void CompileIf(const parser::Test *test);

        void CompileImport(const parser::Import *import);

        void CompileImportAlias(const parser::Binary *alias, bool impfrm);

        void CompileInit(const parser::Initialization *init);

        void CompileJump(const parser::Unary *jump);

        void CompileLoop(const parser::Loop *loop);

        void CompileLTDS(const parser::Unary *list);

        void CompileNullCoalescing(const parser::Binary *binary);

        void CompileSafe(const parser::Unary *unary);

        void CompileSubscr(const parser::Subscript *subscr, bool dup, bool emit);

        void CompileSwitch(const parser::Test *test);

        void CompileSwitchCase(const parser::SwitchCase *sw, BasicBlock **ltest, BasicBlock **lbody,
                               BasicBlock **_default, BasicBlock *end, bool as_if);

        void CompileTernary(const parser::Test *test);

        void CompileTest(const parser::Binary *test);

        void CompileUnary(const parser::Unary *unary);

        void CompileUnpack(vm::datatype::List *list, const scanner::Loc *loc);

        void CompileUpdate(const parser::Unary *update);

        void Expression(const parser::Node *node);

        void IdentifierNew(vm::datatype::String *name, SymbolType stype, vm::datatype::AttributeFlag aflags, bool emit);

        void IdentifierNew(const char *name, SymbolType stype, vm::datatype::AttributeFlag aflags, bool emit);

        void LoadIdentifier(vm::datatype::String *identifier);

        void PushAtom(const char *key, bool emit);

        void StoreVariable(vm::datatype::String *name, const scanner::Loc *loc);

        void TUScopeEnter(vm::datatype::String *name, SymbolType context);

        void TUScopeExit();

    public:
        Compiler() noexcept = default;

        ~Compiler();

        [[nodiscard]] argon::vm::datatype::Code *Compile(argon::lang::parser::File *node);
    };
}

#endif // !ARGON_LANG_COMPILER_H_
