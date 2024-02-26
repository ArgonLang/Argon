// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_COMPILER2_H_
#define ARGON_LANG_COMPILER2_COMPILER2_H_

#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/function.h>

#include <argon/lang/compiler2/transl_unit.h>
#include <argon/lang/compiler2/symt.h>

#include <argon/lang/parser2/parser2.h>

namespace argon::lang::compiler2 {
    constexpr const char *kCompilerErrors[] = {
            "invalid AST node, expected '%s', got: '%s'",
            "invalid NodeType(%d) for %s",
            "invalid TokenType(%d) for %s",
            "cannot use '%s' as identifier",
            "unexpected non named parameter here",
            "unexpected use of 'yield'",
            "invalid token for CompileAugAssignment"
    };

    class Compiler {
        argon::vm::datatype::Dict *static_globals_ = nullptr;

        TranslationUnit *unit_ = nullptr;

        SymbolT *IdentifierLookupOrCreate(String *id, SymbolType type);

        void Compile(const parser2::node::Node *node);

        void CompileAssertion(const parser2::node::Binary *binary);

        void CompileAssignment(const parser2::node::Assignment *assignment);

        void CompileAugAssignment(const parser2::node::Assignment *assignment);

        void CompileFor(const parser2::node::Loop *loop);

        void CompileForEach(const parser2::node::Loop *loop);

        void CompileIF(const parser2::node::Branch *branch);

        void CompileLoop(const parser2::node::Loop *loop);

        void CompileStore(const parser2::node::Node *node, const parser2::node::Node *value);

        void CompileSyncBlock(const parser2::node::Binary *binary);

        void CompileUnpack(List *list, const scanner::Loc *loc);

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

        int CompileSelector(const parser2::node::Binary *binary, bool dup, bool emit);

        int LoadStatic(ArObject *object, const scanner::Loc *loc, bool store, bool emit);

        int LoadStatic(const parser2::node::Unary *literal, bool store, bool emit);

        int LoadStaticAtom(const char *key, const scanner::Loc *loc, bool emit);

        int LoadStaticNil(const scanner::Loc *loc, bool emit);

        String *MakeFnName();

        void IdentifierNew(String *name, const scanner::Loc *loc, SymbolType type, AttributeFlag aflags, bool emit);

        void IdentifierNew(const parser2::node::Unary *id, SymbolType type, AttributeFlag aflags, bool emit);

        void LoadIdentifier(String *identifier, const scanner::Loc *loc);

        void LoadIdentifier(const parser2::node::Unary *identifier);

        void CompileBlock(const parser2::node::Node *node, bool sub);

        void CompileCall(const parser2::node::Call *call);

        void CompileCallKWArgs(List *args, unsigned short &count, vm::OpCodeCallMode &mode);

        void CompileCallPositional(List *args, unsigned short &count, vm::OpCodeCallMode &mode);

        void CompileDLST(const parser2::node::Unary *unary);

        void CompileElvis(const parser2::node::Binary *binary);

        void CompileFunction(const parser2::node::Function *func);

        void CompileFunctionClosure(const Code *code, const scanner::Loc *loc, FunctionFlags &flags);

        void CompileFunctionDefArgs(List *params, const scanner::Loc *loc, FunctionFlags &flags);

        void CompileFunctionDefBody(const parser2::node::Function *func, String *name);

        void CompileFunctionParams(vm::datatype::List *params, unsigned short &count, FunctionFlags &flags);

        void CompileInfix(const parser2::node::Binary *binary);

        void CompileNullCoalescing(const parser2::node::Binary *binary);

        void CompileObjInit(const parser2::node::ObjectInit *init);

        void CompilePrefix(const parser2::node::Unary *unary);

        void CompileSafe(const parser2::node::Unary *unary);

        void CompileSubscr(const parser2::node::Subscript *subscr, bool dup, bool emit);

        void CompileTest(const parser2::node::Binary *binary);

        void CompileTernary(const parser2::node::Branch *branch);

        void CompileTrap(const parser2::node::Unary *unary);

        void CompileUpdate(const parser2::node::Unary *unary);

        void Expression(const parser2::node::Node *node);

        void StoreVariable(String *id, const scanner::Loc *loc);

        void StoreVariable(const parser2::node::Unary *identifier);

// *********************************************************************************************************************
// PRIVATE
// *********************************************************************************************************************

        void EnterScope(vm::datatype::String *name, SymbolType type);

        void ExitScope();

    public:
        Compiler() noexcept = default;

        [[nodiscard]] vm::datatype::Code *Compile(argon::lang::parser2::node::Module *mod);
    };
} // argon::lang::compiler2

#endif // ARGON_LANG_COMPILER2_COMPILER2_H_
