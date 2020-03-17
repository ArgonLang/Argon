// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <istream>

#include "basicblock.h"
#include "opcodes.h"
#include "ast/ast.h"

namespace lang {
    class Compiler {
        BasicBlock *bb_start_ = nullptr;
        BasicBlock *bb_list = nullptr;
        BasicBlock *bb_curr_ = nullptr;

        void EmitOp(OpCodes code, unsigned char arg);

        void CompileBinaryExpr(const ast::Binary *binary);

        void CompileBranch(const ast::If *stmt);

        void CompileCode(const ast::NodeUptr &node);

        void CompileCompound(const ast::List *list);

        void CompileLiteral(const ast::Literal *literal);

        void CompileTest(const ast::Binary *test);

        void CompileUnaryExpr(const ast::Unary *unary);

        void UseAsNextBlock(BasicBlock *block);

        BasicBlock *NewBlock();

        BasicBlock *NewNextBlock();

    public:
        void Compile(std::istream *source);
    };

} // namespace lang

#endif // !ARGON_LANG_COMPILER_H_
