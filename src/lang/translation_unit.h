// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_TRANSLATION_UNIT_H_
#define ARGON_LANG_TRANSLATION_UNIT_H_

#include <cassert>

#include <vm/opcode.h>

#include <vm/datatype/arstring.h>
#include <vm/datatype/code.h>
#include <vm/datatype/dict.h>
#include <vm/datatype/list.h>

#include <lang/scanner/token.h>

#include "basicblock.h"
#include "symt.h"

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

        vm::datatype::Code *Assemble() const;

        JBlock *JBNew(vm::datatype::String *label);

        JBlock *JBNew(vm::datatype::String *label, BasicBlock *end);

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
            int combined = flags << 16 | arg;
            this->Emit(opcode, combined, nullptr, loc);
        }

        void IncrementStack(int size) {
            this->stack.current += size;
            if (this->stack.current > this->stack.required)
                this->stack.required = this->stack.current;
        }

        void JBPop(const JBlock *block);
    };

    TranslationUnit *TranslationUnitNew(TranslationUnit *prev, vm::datatype::String *name, SymbolT *symt);

    TranslationUnit *TranslationUnitDel(TranslationUnit *unit);
}

#endif // !ARGON_LANG_TRANSLATION_UNIT_H_
