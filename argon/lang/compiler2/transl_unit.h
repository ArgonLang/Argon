// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_TRANSL_UNIT_H_
#define ARGON_LANG_COMPILER2_TRANSL_UNIT_H_

#include <argon/vm/datatype/arstring.h>

#include <argon/lang/scanner/token.h>

#include <argon/lang/compiler2/basicblock.h>
#include <argon/lang/compiler2/jblock.h>
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

        JBlock *jblock;

        BasicBlockSeq bbb; // It should be called 'bb', but this is a joke for M.G =)

        struct {
            unsigned int required;
            unsigned int current;
        } stack;

        struct {
            unsigned short required;
            unsigned short current;
        } sync_stack;

        unsigned int anon_count;

        BasicBlock *BlockNew();

        BasicBlock *BlockAppend(BasicBlock *block);

        JBlock *JBFindLabel(const vm::datatype::String *label, unsigned short &out_pops) const;

        JBlock *JBPush(vm::datatype::String *label, BasicBlock *begin, BasicBlock *end, JBlockType type);

        JBlock *JBPush(vm::datatype::String *label, JBlockType type);

        JBlock *JBPush(BasicBlock *begin, BasicBlock *end);

        JBlock *JBPush(BasicBlock *begin, BasicBlock *end, unsigned short pops);

        bool CheckBlock(JBlockType expected) const;

        bool IsFreeVar(const vm::datatype::String *id) const;

        void JBPop();

        void DecrementStack(int size) {
            this->stack.current -= size;
            assert(this->stack.current < 0x00FFFFFF);
        }

        void Emit(vm::OpCode op, int arg, BasicBlock *dest, const scanner::Loc *loc);

        void Emit(vm::OpCode op, const scanner::Loc *loc) {
            this->Emit(op, 0, nullptr, loc);
        }

        void Emit(vm::OpCode op, unsigned char flags, unsigned short arg, const scanner::Loc *loc) {
            int combined = (flags << 16u) | arg;
            this->Emit(op, combined, nullptr, loc);
        }

        void EmitPOP() {
            this->Emit(vm::OpCode::POP, 0, nullptr, nullptr);
        }

        void EnterSync(const scanner::Loc *loc) {
            this->Emit(vm::OpCode::SYNC, loc);

            this->sync_stack.current++;
            if (this->sync_stack.current > this->sync_stack.required)
                this->sync_stack.required = this->sync_stack.current;
        }

        void ExitSync() {
            this->Emit(vm::OpCode::UNSYNC, nullptr);

            this->sync_stack.current--;

            assert(this->sync_stack.current < 0xFF);
        }

        void IncrementStack(int size) {
            this->stack.current += size;

            if (this->stack.current > this->stack.required)
                this->stack.required = this->stack.current;
        }
    };

    TranslationUnit *TranslationUnitNew(TranslationUnit *prev, vm::datatype::String *name, SymbolType type);

    TranslationUnit *TranslationUnitDel(TranslationUnit *unit);

} // argon::lang::compiler2

#endif // !ARGON_LANG_COMPILER2_TRANSL_UNIT_H_
