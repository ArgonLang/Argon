// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "compilererr.h"
#include "translation_unit.h"

using namespace argon::lang;
using namespace argon::vm::datatype;

bool MakeQName(const TranslationUnit *prev, TranslationUnit *unit, String *name) {
    const char *sep = "%s.%s";

    if (prev != nullptr && prev->qname != nullptr && name != nullptr && !StringIsEmpty(name)) {
        if (prev->symt->type != SymbolType::MODULE)
            sep = "%s::%s";

        if ((unit->qname = StringFormat(sep, ARGON_RAW_STRING(prev->qname), ARGON_RAW_STRING(name))) == nullptr)
            return false;
    } else
        unit->qname = IncRef(name);

    unit->name = IncRef(name);
    return true;
}

void BlockAppend(TranslationUnit *unit, BasicBlock *block) {
    if (unit->bb.start == nullptr) {
        unit->bb.start = block;
        unit->bb.cur = block;
    } else {
        unit->bb.cur->next = block;
        unit->bb.cur = block;
    }
}

bool TranslationUnit::BlockNew() {
    auto *block = BasicBlockNew();
    if (block == nullptr)
        return false;

    ::BlockAppend(this, block);
    return true;
}

bool TranslationUnit::IsFreeVar(String *id) {
    // Look back in the TranslationUnits,
    // if a variable with the same name exists and is declared or free
    // in turn then this is a free variable
    SymbolT *sym;

    for (TranslationUnit *tu = this->prev; tu != nullptr && tu->symt->type == SymbolType::FUNC; tu = tu->prev) {
        sym = SymbolLookup(tu->symt, id);
        if (sym != nullptr) {
            if (sym->declared || sym->free) {
                Release(sym);
                return true;
            }
            Release(sym);
        }
    }

    return false;
}

Code *TranslationUnit::Assemble(String *docstring) const {
    Code *code;

    unsigned char *instr_buf;
    unsigned char *instr_cur;

    unsigned int instr_sz = 0;

    for (BasicBlock *cursor = this->bb.start; cursor != nullptr; cursor = cursor->next) {
        cursor->offset = instr_sz;
        instr_sz += cursor->size;
    }

    if (instr_sz == 0) {
        code = CodeNew(nullptr, nullptr, this->statics, this->names, this->locals,
                       this->enclosed, 0, this->stack.required);
        if (code == nullptr)
            throw DatatypeException();

        return code;
    }

    if ((instr_buf = (unsigned char *) vm::memory::Alloc(instr_sz)) == nullptr)
        throw DatatypeException();

    instr_cur = instr_buf;

    for (BasicBlock *cursor = this->bb.start; cursor != nullptr; cursor = cursor->next) {
        for (Instr *instr = cursor->instr.head; instr != nullptr; instr = instr->next) {
            auto arg = instr->oparg & 0x00FFFFFF; // extract arg

            if (instr->jmp != nullptr)
                arg = instr->jmp->offset;

            switch (vm::OpCodeOffset[instr->opcode]) {
                case 4:
                    *((argon::vm::Instr32 *) instr_cur) = arg << 8 | instr->opcode;
                    instr_cur += 4;
                    break;
                case 2:
                    *((argon::vm::Instr16 *) instr_cur) = (argon::vm::Instr16) (arg << 8 | instr->opcode);
                    instr_cur += 2;
                    break;
                default:
                    *instr_cur = instr->opcode;
                    instr_cur++;
            }
        }
    }

    code = CodeNew(instr_buf, docstring, this->statics, this->names, this->locals,
                   this->enclosed, instr_sz, this->stack.required);
    if (code == nullptr) {
        vm::memory::Free(instr_buf);
        throw DatatypeException();
    }

    return code;
}

JBlock *TranslationUnit::JBNew(String *label) {
    BasicBlock *begin = this->bb.cur;
    JBlock *block = this->jstack;

    for (; block != nullptr; block = block->prev) {
        if (label != nullptr &&
            block->label != nullptr &&
            StringCompare(block->label, label) == 0 &&
            block->nested == this->symt->nested)
            break;
    }

    if (block == nullptr) {
        if (this->bb.cur->size > 0) {
            if ((begin = BasicBlockNew()) == nullptr)
                throw DatatypeException();
            ::BlockAppend(this, begin);
        }

        if ((block = JBlockNew(this->jstack, label, this->symt->nested)) == nullptr)
            throw DatatypeException();

        block->start = begin;
        this->jstack = block;
    }

    return block;
}

JBlock *TranslationUnit::JBNew(String *label, BasicBlock *end) {
    auto *jb = this->JBNew(label);

    jb->end = end;

    return jb;
}

JBlock *TranslationUnit::JBNew(BasicBlock *start, BasicBlock *end) {
    String *label = nullptr;

    if (this->jstack != nullptr) {
        if (this->jstack->start->size == 0)
            label = this->jstack->label;
    }

    auto *jb = this->JBNew(label);

    jb->start = start;
    jb->end = end;

    jb->loop = true;

    return jb;
}

JBlock *TranslationUnit::FindLoop(String *label) {
    for (JBlock *block = this->jstack; block != nullptr; block = block->prev) {
        if (!block->loop)
            continue;

        if (label == nullptr || (block->label != nullptr && StringCompare(block->label, label) == 0))
            return block;
    }

    return nullptr;
}

void TranslationUnit::BlockAppend(argon::lang::BasicBlock *block) {
    ::BlockAppend(this, block);
}

void TranslationUnit::Emit(vm::OpCode opcode, int arg, BasicBlock *dest, const scanner::Loc *loc) {
    auto *instr = this->bb.cur->AddInstr(opcode, arg);
    if (instr != nullptr) {
        instr->jmp = dest;

        if (loc != nullptr)
            instr->lineno = (unsigned int) loc->start.line;

        assert(((unsigned char) opcode) < sizeof(vm::StackChange));

        this->IncrementStack(vm::StackChange[(unsigned char) opcode]);

        return;
    }

    throw DatatypeException();
}

void TranslationUnit::JBPop(const JBlock *block) {
    JBlock *tmp = this->jstack;

    if (tmp == block) {
        this->jstack = JBlockDel(tmp);
        return;
    }

    for (JBlock *cur = this->jstack->prev; cur != nullptr; cur = tmp) {
        if (cur == block) {
            tmp->prev = JBlockDel(cur);
            break;
        }
        tmp = cur;
    }
}

TranslationUnit *argon::lang::TranslationUnitNew(TranslationUnit *prev, String *name, SymbolT *symt) {
    auto *tu = (TranslationUnit *) argon::vm::memory::Calloc(sizeof(TranslationUnit));

    if (tu != nullptr) {
        if (symt->type == SymbolType::STRUCT || symt->type == SymbolType::TRAIT)
            vm::memory::MemoryCopy(tu, prev, sizeof(TranslationUnit));

        tu->prev = prev;

        tu->symt = IncRef(symt);

        if (!MakeQName(prev, tu, name)) {
            argon::vm::memory::Free(tu);
            return nullptr;
        }

        if (symt->type != SymbolType::STRUCT && symt->type != SymbolType::TRAIT) {
            if ((tu->statics_map = DictNew()) == nullptr)
                goto ERROR;

            if ((tu->statics = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->names = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->locals = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->enclosed = ListNew()) == nullptr)
                goto ERROR;

            auto *block = BasicBlockNew();
            if (block == nullptr)
                goto ERROR;

            BlockAppend(tu, block);
        }

        tu->anon_count_ = 0;
    }

    return tu;

    ERROR:
    Release(tu->symt);
    Release(tu->name);
    Release(tu->qname);
    Release(tu->statics_map);
    Release(tu->statics);
    Release(tu->names);
    Release(tu->locals);

    argon::vm::memory::Free(tu);

    return nullptr;
}

TranslationUnit *argon::lang::TranslationUnitDel(TranslationUnit *unit) {
    auto *prev = unit->prev;

    if (unit->symt->type == SymbolType::STRUCT || unit->symt->type == SymbolType::TRAIT) {
        auto stack = unit->stack.required + prev->stack.current;

        prev->bb.cur = unit->bb.cur;

        if (prev->stack.required < stack)
            prev->stack.required = stack;

        argon::vm::memory::Free(unit);

        return prev;
    }

    // Free all BasicBlock
    BasicBlock *tmp = unit->bb.start;
    while ((tmp = BasicBlockDel(tmp)) != nullptr);

    Release(unit->symt);
    Release(unit->name);
    Release(unit->qname);
    Release(unit->statics_map);
    Release(unit->statics);
    Release(unit->names);
    Release(unit->locals);

    argon::vm::memory::Free(unit);

    return prev;
}