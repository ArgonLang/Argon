// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_TRANSL_UNIT_H_
#define ARGON_LANG_COMPILER2_TRANSL_UNIT_H_

#include <argon/vm/datatype/arstring.h>

#include <argon/lang/scanner/token.h>

#include <argon/lang/compiler2/basicblock.h>
#include <argon/lang/compiler2/symt.h>

namespace argon::lang::compiler2 {
    struct TranslationUnit {
        TranslationUnit *prev;

        /// Pointer to current scope SymbolTable.
        SymbolT *symt;

        /// Name of translation unit.
        vm::datatype::String *name;

        /// Qualified name of translation unit.
        vm::datatype::String *qname;

        /// Local statics dict.
        vm::datatype::Dict *statics_map;

        /// Static resources.
        vm::datatype::List *statics;

        /// External variables (global scope).
        vm::datatype::List *names;

        /// Local variables (function/cycle scope).
        vm::datatype::List *locals;

        /// Closure.
        vm::datatype::List *enclosed;

        BasicBlockSeq bbb; // It should be called 'bb', but this is a joke for M.G =)

        BasicBlock *BlockNew();

        BasicBlock *BlockAppend(BasicBlock *block);

        void Emit(vm::OpCode op, int arg, BasicBlock *dest, const scanner::Loc *loc);

        void Emit(vm::OpCode op, const scanner::Loc *loc) {
            this->Emit(op, 0, nullptr, loc);
        }
    };

    TranslationUnit *TranslationUnitNew(TranslationUnit *prev, vm::datatype::String *name, SymbolType type);

    TranslationUnit *TranslationUnitDel(TranslationUnit *unit);

} // argon::lang::compiler2

#endif // !ARGON_LANG_COMPILER2_TRANSL_UNIT_H_
