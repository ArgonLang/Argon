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
#include <object/function.h>

#include "basicblock.h"
#include "opcodes.h"
#include "ast/ast.h"
#include "lang/symt/symbol_table.h"
#include "compile_unit.h"

namespace lang {
    class Compiler {
        argon::object::Map *statics_global;

        std::list<CompileUnit> cu_list_;
        CompileUnit *cu_curr_ = nullptr;

        BasicBlock *NewBlock();

        BasicBlock *NewNextBlock();

        bool PushStatic(argon::object::ArObject *obj, bool store, bool emit_op, unsigned int *out_idx);

        bool PushStatic(const std::string &value, bool emit_op, unsigned int *out_idx);

        argon::object::Code *Assemble();

        argon::object::Function *AssembleFunction(const ast::Function *function);

        void CompileAssignment(const ast::Assignment *assign);

        void CompileBinaryExpr(const ast::Binary *binary);

        void CompileBranch(const ast::If *stmt);

        void CompileCode(const ast::NodeUptr &node);

        void CompileCompound(const ast::List *list);

        void CompileForLoop(const ast::For *loop);

        void CompileFunction(const ast::Function *function);

        void CompileLiteral(const ast::Literal *literal);

        void CompileLoop(const ast::Loop *loop);

        void CompileMember(const ast::Member *member);

        void CompileSlice(const ast::Slice *slice);

        void CompileSubscr(const ast::Binary *subscr, const ast::NodeUptr &assignable);

        void CompileSwitch(const ast::Switch *stmt, bool as_if);

        void CompileTest(const ast::Binary *test);

        void CompileTrait(const ast::Construct *trait);

        void CompileUnaryExpr(const ast::Unary *unary);

        void CompileVariable(const ast::Variable *variable);

        void DecEvalStack(int value);

        void Dfs(CompileUnit *unit, BasicBlock *start);

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EmitOp4Flags(OpCodes code, unsigned char flags, unsigned short arg);

        void EnterScope(const std::string &scope_name, CUScope scope);

        void ExitScope();

        void IncEvalStack();

        void LoadVariable(const std::string &name);

        void NewVariable(const std::string &name, bool emit_op, unsigned char flags);

        void UseAsNextBlock(BasicBlock *block);

    public:
        Compiler();

        ~Compiler();

        argon::object::Code *Compile(std::istream *source);
    };

} // namespace lang

#endif // !ARGON_LANG_COMPILER_H_
