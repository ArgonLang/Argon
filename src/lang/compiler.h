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

        bool IsFreeVariable(const std::string &name);

        void CompileCode(const ast::NodeUptr &node);

        void CompileBinary(const ast::Binary *binary);

        void CompileLiteral(const ast::Literal *literal);

        void EnterContext(const std::string &name, TUScope scope);

        void ExitContext();

        void EmitOp(OpCodes code);

        void EmitOp2(OpCodes code, unsigned char arg);

        void EmitOp4(OpCodes code, unsigned int arg);

        void EmitOp4Flags(OpCodes code, unsigned char flags, unsigned short arg);

        void VariableNew(const std::string &name, bool emit, unsigned char flags);

        void VariableLoad(const std::string &name);

        unsigned int PushStatic(argon::object::ArObject *obj, bool store, bool emit);

    public:
        Compiler();

        explicit Compiler(argon::object::Map *statics_globals);

        ~Compiler();

        argon::object::Code *Compile(std::istream *source);
    };

} // namespace argon::lang

#endif // !ARGON_LANG_COMPILER2_H_
