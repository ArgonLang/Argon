// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_TRANSLATION_UNIT_H_
#define ARGON_LANG_TRANSLATION_UNIT_H_

#include <cassert>

#include <argon/vm/opcode.h>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/list.h>

#include <argon/lang/scanner/token.h>

#include <argon/lang/basicblock.h>
#include <argon/lang/symt.h>

namespace argon::lang {
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

        JBlock *jstack;

        struct {
            BasicBlock *start;
            BasicBlock *cur;
        } bb;

        struct {
            unsigned int required;
            unsigned int current;
        } stack;

        unsigned int anon_count_;

        bool BlockNew();

        bool IsFreeVar(vm::datatype::String *id);

        [[nodiscard]] vm::datatype::Code *Assemble(vm::datatype::String *docstring) const;

        JBlock *JBNew(vm::datatype::String *label);

        JBlock *JBNew(vm::datatype::String *label, BasicBlock *end);

        JBlock *JBNew(BasicBlock *start, BasicBlock *end, unsigned short pops);

        JBlock *FindLoop(vm::datatype::String *label);

        void BlockAppend(BasicBlock *block);

        void DecrementStack(int size) {
            this->stack.current -= size;
            assert(this->stack.current < 0x00FFFFFF);
        }

        void Emit(vm::OpCode opcode, int arg, BasicBlock *dest, const scanner::Loc *loc);

        void Emit(vm::OpCode opcode, const scanner::Loc *loc) {
            this->Emit(opcode, 0, nullptr, loc);
        }

        void Emit(vm::OpCode opcode, BasicBlock *dest, const scanner::Loc *loc) {
            this->Emit(opcode, 0, dest, loc);
        }

        void Emit(vm::OpCode opcode, unsigned char flags, unsigned short arg, const scanner::Loc *loc) {
            int combined = (flags << 16) | arg;
            this->Emit(opcode, combined, nullptr, loc);
        }

        void IncrementRequiredStack(int size) {
            if (this->stack.current + size > this->stack.required)
                this->stack.required = this->stack.current + size;
        }

        void IncrementStack(int size) {
            this->stack.current += size;
            if (this->stack.current > this->stack.required)
                this->stack.required = this->stack.current;
        }

        void JBPop(JBlock *block);
    };

    TranslationUnit *TranslationUnitNew(TranslationUnit *prev, vm::datatype::String *name, SymbolT *symt);

    TranslationUnit *TranslationUnitDel(TranslationUnit *unit);
}

#endif // !ARGON_LANG_TRANSLATION_UNIT_H_
