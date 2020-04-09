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

namespace lang {
    struct CompileUnit {
        SymTUptr symt;
        argon::object::Map statics_map;

        argon::object::List *statics;
        argon::object::List *names;
        argon::object::List *locals;

        std::vector<BasicBlock *> bb_splist;

        BasicBlock *bb_start = nullptr;
        BasicBlock *bb_list = nullptr;
        BasicBlock *bb_curr = nullptr;

        CompileUnit *prev = nullptr;

        unsigned int instr_sz = 0;
        unsigned int stack_sz = 0;
        unsigned int stack_cu_sz = 0;

        CompileUnit() {
            this->statics = argon::object::ListNew();
            this->names = argon::object::ListNew();
            this->locals = argon::object::ListNew();
        }

        ~CompileUnit() {
            for (BasicBlock *cursor = this->bb_list, *nxt; cursor != nullptr; cursor = nxt) {
                nxt = cursor->link_next;
                delete (cursor);
            }
            argon::object::Release(this->statics);
            argon::object::Release(this->names);
            argon::object::Release(this->locals);
        }
    };

    class Compiler {
        argon::object::Map statics_global;

        std::list<CompileUnit> cu_list_;
        CompileUnit *cu_curr_ = nullptr;

        argon::object::Code *Assemble();

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EnterScope();

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
        argon::object::Code *Compile(std::istream *source);
    };

} // namespace lang

#endif // !ARGON_LANG_COMPILER_H_
