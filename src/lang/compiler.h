// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

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

        argon::object::Code *CompileFunction(const ast::Function *func);

        bool IsFreeVariable(const std::string &name);

        bool VariableLookupCreate(const std::string &name, Symbol **out_sym);

        unsigned int CompileMember(const ast::Member *member, bool duplicate, bool emit_last);

        unsigned int CompileScope(const ast::Scope *scope, bool duplicate, bool emit_last);

        unsigned int PushStatic(const std::string &value, bool store, bool emit);

        unsigned int PushStatic(argon::object::ArObject *obj, bool store, bool emit);

        void CompileAssignment(const ast::Assignment *assign);

        void CompileAugAssignment(const ast::Assignment *assign);

        void CompileBinary(const ast::Binary *binary);

        void CompileBranch(const ast::If *stmt);

        void CompileCall(const ast::Call *call, OpCodes code);

        void CompileCode(const ast::NodeUptr &node);

        void CompileCompound(const ast::List *list);

        void CompileConstruct(const ast::Construct *construct);

        void CompileForLoop(const ast::For *loop, const std::string &name);

        void CompileImport(const ast::Import *import);

        void CompileImportFrom(const ast::Import *import);

        void CompileJump(OpCodes op, BasicBlock *src, BasicBlock *dest);

        void CompileJump(OpCodes op, BasicBlock *dest);

        void CompileLiteral(const ast::Literal *literal);

        void CompileLoop(const ast::Loop *loop, const std::string &name);

        void CompileSlice(const ast::Slice *slice);

        void CompileStructInit(const ast::StructInit *init);

        void CompileSubscr(const ast::Binary *subscr, bool duplicate, bool emit_subscr);

        void CompileSwitch(const ast::Switch *stmt, const std::string &name);

        void CompileTest(const ast::Binary *test);

        void CompileUpdate(const ast::Update *update);

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EmitOp4Flags(OpCodes code, unsigned char flags, unsigned short arg);

        void EnterContext(const std::string &name, TUScope scope);

        void ExitContext();

        void VariableLoad(const std::string &name);

        void VariableNew(const std::string &name, bool emit, unsigned char flags);

        void VariableStore(const std::string &name);

    public:
        Compiler();

        explicit Compiler(argon::object::Map *statics_globals);

        ~Compiler();

        argon::object::Code *Compile(std::istream *source);
    };

} // namespace argon::lang

#endif // !ARGON_LANG_COMPILER_H_
