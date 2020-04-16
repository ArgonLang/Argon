// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <istream>
#include <vector>
#include <object/list.h>
#include <object/map.h>
#include <object/code.h>

#include "basicblock.h"
#include "opcodes.h"
#include "ast/ast.h"
#include "symbol_table.h"
#include "compile_unit.h"

namespace lang {
    class Compiler {
        argon::object::Map *statics_global;

        std::list<CompileUnit> cu_list_;
        CompileUnit *cu_curr_ = nullptr;

        argon::object::Code *Assemble();

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EnterScope(const std::string &scope_name, CUScope scope);

        void ExitScope();

        void CompileBinaryExpr(const ast::Binary *binary);

        void CompileBranch(const ast::If *stmt);

        void CompileCode(const ast::NodeUptr &node);

        void CompileLoop(const ast::Loop *loop);

        void CompileSwitch(const ast::Switch *stmt, bool as_if);

        void CompileVariable(const ast::Variable *variable);

        void CompileAssignment(const ast::Assignment *assign);

        void CompileCompound(const ast::List *list);

        void CompileIdentifier(const ast::Identifier *identifier);

        void CompileLiteral(const ast::Literal *literal);

        void CompileTest(const ast::Binary *test);

        void CompileUnaryExpr(const ast::Unary *unary);

        void Dfs(CompileUnit *unit, BasicBlock *start);

        void UseAsNextBlock(BasicBlock *block);

        void IncEvalStack();

        void DecEvalStack();

        BasicBlock *NewBlock();

        BasicBlock *NewNextBlock();

    public:
        Compiler();

        ~Compiler();

        argon::object::Code *Compile(std::istream *source);
    };

} // namespace lang

#endif // !ARGON_LANG_COMPILER_H_
