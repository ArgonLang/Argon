// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <istream>
#include <vector>
#include <object/list.h>

#include "basicblock.h"
#include "opcodes.h"
#include "ast/ast.h"
#include "symbol_table.h"

namespace lang {
    struct CompileUnit {
        SymTUptr symt;

        argon::object::List *constants;
        std::vector<std::string> names;
        std::vector<std::string> locals;

        std::vector<BasicBlock *> bb_splist;
        BasicBlock *bb_start = nullptr;
        BasicBlock *bb_list = nullptr;
        BasicBlock *bb_curr = nullptr;

        CompileUnit *prev = nullptr;

        size_t instr_sz = 0;

        CompileUnit() : constants(argon::object::NewObject<argon::object::List>()) {}

        ~CompileUnit() {
            for (BasicBlock *cursor = this->bb_list, *nxt; cursor != nullptr; cursor = nxt) {
                nxt = cursor->link_next;
                delete (cursor);
            }
            argon::object::ReleaseObject(this->constants);
        }
    };

    class Compiler {
        std::list<CompileUnit> cu_list_;
        CompileUnit *cu_curr_ = nullptr;

        void Assemble();

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EnterScope();

        void CompileBinaryExpr(const ast::Binary *binary);

        void CompileBranch(const ast::If *stmt);

        void CompileCode(const ast::NodeUptr &node);

        void CompileLoop(const ast::Loop *loop);

        void CompileSwitch(const ast::Switch *stmt);

        void CompileSwitchAsIf(const ast::Switch *stmt);

        void CompileVariable(const ast::Variable *variable);

        void CompileCompound(const ast::List *list);

        void CompileIdentifier(const ast::Identifier *identifier);

        void CompileLiteral(const ast::Literal *literal);

        void CompileTest(const ast::Binary *test);

        void CompileUnaryExpr(const ast::Unary *unary);

        void Dfs(CompileUnit *unit, BasicBlock *start);

        void UseAsNextBlock(BasicBlock *block);

        BasicBlock *NewBlock();

        BasicBlock *NewNextBlock();

    public:
        void Compile(std::istream *source);
    };

} // namespace lang

#endif // !ARGON_LANG_COMPILER_H_
