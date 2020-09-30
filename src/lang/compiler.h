// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_H_
#define ARGON_LANG_COMPILER2_H_

#include <istream>

#include <object/arobject.h>
#include <object/datatype/code.h>
#include <object/datatype/map.h>

#include <lang/ast/ast.h>
#include "transl_unit.h"

namespace argon::lang {

    class Compiler {
        argon::object::Map *statics_globals_;

        TranslationUnit *unit_;

        argon::object::Code *Assemble();

        bool IsFreeVariable(const std::string &name);

        void CompileCode(const ast::NodeUptr &node);

        void CompileUpdate(const ast::Update *update);

        void CompileConstruct(const ast::Construct *construct);

        void CompileCall(const ast::Call *call, OpCodes code);

        void CompileSubscr(const ast::Binary *subscr, bool duplicate, bool emit_subscr);

        unsigned int CompileMember(const ast::Member *member, bool duplicate, bool emit_last);

        unsigned int CompileScope(const ast::Scope *scope, bool duplicate, bool emit_last);

        void CompileBranch(const ast::If *stmt);

        void CompileBinary(const ast::Binary *binary);

        void CompileLoop(const ast::Loop *loop, const std::string &name);

        void CompileForLoop(const ast::For *loop, const std::string &name);

        void CompileImport(const ast::Import *import);

        void CompileImportFrom(const ast::Import *import);

        void CompileSwitch(const ast::Switch *stmt, const std::string &name);

        void CompileSlice(const ast::Slice *slice);

        void CompileAugAssignment(const ast::Assignment *assign);

        void CompileAssignment(const ast::Assignment *assign);

        void CompileCompound(const ast::List *list);

        argon::object::Code *CompileFunction(const ast::Function *func);

        void CompileJump(OpCodes op, BasicBlock *src, BasicBlock *dest);

        void CompileJump(OpCodes op, BasicBlock *dest);

        void CompileTest(const ast::Binary *test);

        void CompileLiteral(const ast::Literal *literal);

        void EnterContext(const std::string &name, TUScope scope);

        void ExitContext();

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EmitOp4Flags(OpCodes code, unsigned char flags, unsigned short arg);

        void VariableNew(const std::string &name, bool emit, unsigned char flags);

        void VariableLoad(const std::string &name);

        void VariableStore(const std::string &name);

        bool VariableLookupCreate(const std::string &name, Symbol **out_sym);

        unsigned int PushStatic(const std::string &value, bool store, bool emit);

        unsigned int PushStatic(argon::object::ArObject *obj, bool store, bool emit);

    public:
        Compiler();

        explicit Compiler(argon::object::Map *statics_globals);

        ~Compiler();

        argon::object::Code *Compile(std::istream *source);
    };

} // namespace argon::lang

#endif // !ARGON_LANG_COMPILER2_H_
