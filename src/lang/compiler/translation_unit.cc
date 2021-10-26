// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>
#include <vm/runtime.h>
#include <object/datatype/error.h>

#include "translation_unit.h"

using namespace argon::lang::compiler;
using namespace argon::object;

bool MakeQName(TranslationUnit *prev, TranslationUnit *unit, String *name) {
    String *tmp;
    String *sep;

    if (prev != nullptr && name != nullptr && !StringEmpty(name)) {
        if ((sep = StringIntern("::")) == nullptr)
            return false;

        if ((tmp = StringConcat(prev->name, sep)) == nullptr) {
            Release(sep);
            return false;
        }

        Release(sep);

        if ((unit->qname = StringConcat(tmp, name)) == nullptr) {
            Release(tmp);
            return false;
        }

        Release(tmp);
    } else
        unit->qname = IncRef(name);

    unit->name = IncRef(name);
    return true;
}

bool argon::lang::compiler::TranslationUnitIsFreeVar(TranslationUnit *unit, String *name) {
    // Look back in the TranslationUnits,
    // if a variable with the same name exists and is declared or free
    // in turn then this is a free variable
    Symbol *sym;

    for (TranslationUnit *tu = unit; tu != nullptr && tu->scope == TUScope::FUNCTION; tu = tu->prev) {
        sym = SymbolTableLookup(unit->symt, name);
        if (sym != nullptr) {
            if ((sym->declared || sym->free)) {
                Release(sym);
                return true;
            }
            Release(sym);
        }
    }

    return false;
}

Code *argon::lang::compiler::TranslationUnitAssemble(TranslationUnit *unit) {
    unsigned int instr_sz = 0;
    unsigned int arg;
    unsigned char *buf;
    unsigned char *bcur;

    Code *code;

    for (BasicBlock *cursor = unit->bb.start; cursor != nullptr; cursor = cursor->next) {
        cursor->i_offset = instr_sz;
        instr_sz += cursor->i_size;
    }

    if ((buf = (unsigned char *) argon::memory::Alloc(instr_sz)) == nullptr)
        return (Code *) argon::vm::Panic(error_out_of_memory);

    bcur = buf;
    for (BasicBlock *cursor = unit->bb.start; cursor != nullptr; cursor = cursor->next) {
        for (Instr *instr = cursor->instr.head; instr != nullptr; instr = instr->next) {
            arg = instr->oparg & 0x00FFFFFF; // extract arg

            if (instr->jmp != nullptr)
                arg = instr->jmp->i_offset;

            *((Instr32 *) bcur) = arg << 8 | instr->opcode;
            bcur += (instr->oparg & 0xFF000000) >> 24u;
        }
    }

    code = CodeNew(buf, instr_sz, unit->stack.required, unit->statics, unit->names, unit->locals, unit->enclosed);

    if (code == nullptr)
        argon::memory::Free(buf);

    return code;
}

TranslationUnit *
argon::lang::compiler::TranslationUnitNew(TranslationUnit *prev, String *name, TUScope scope, SymbolTable *symt) {
    auto *tu = (TranslationUnit *) argon::memory::Alloc(sizeof(TranslationUnit));

    if (tu == nullptr) {
        argon::vm::Panic(argon::object::error_out_of_memory);
        return nullptr;
    }

    argon::memory::MemoryZero(tu, sizeof(TranslationUnit));

    if ((tu->symt = symt) == nullptr)
        goto ERROR;

    if ((tu->statics_map = MapNew()) == nullptr)
        goto ERROR;

    if ((tu->statics = ListNew()) == nullptr)
        goto ERROR;

    if ((tu->names = ListNew()) == nullptr)
        goto ERROR;

    if ((tu->locals = ListNew()) == nullptr)
        goto ERROR;

    if ((tu->enclosed = ListNew()) == nullptr)
        goto ERROR;

    if (TranslationUnitBlockNew(tu) == nullptr)
        goto ERROR;

    if (!MakeQName(prev, tu, name))
        goto ERROR;

    tu->jstack = nullptr;
    tu->scope = scope;
    tu->anon_count = 0;
    tu->prev = prev;

    return tu;

    ERROR:
    TranslationUnitDel(tu);
    return nullptr;
}

TranslationUnit *argon::lang::compiler::TranslationUnitDel(TranslationUnit *unit) {
    TranslationUnit *prev;
    BasicBlock *tmp;
    JBlock *jb;

    if (unit == nullptr)
        return nullptr;

    prev = unit->prev;
    tmp = unit->bb.start;
    jb = unit->jstack;

    // Free all BasicBlock
    while ((tmp = BasicBlockDel(tmp)) != nullptr);

    // Free all JBlock
    while ((jb = JBlockDel(jb)) != nullptr);

    Release(unit->name);
    Release(unit->qname);
    Release(unit->statics_map);
    Release(unit->statics);
    Release(unit->names);
    Release(unit->locals);
    Release(unit->enclosed);
    argon::memory::Free(unit);

    return prev;
}

BasicBlock *argon::lang::compiler::TranslationUnitBlockNew(TranslationUnit *unit) {
    auto *bb = BasicBlockNew();

    if (bb != nullptr)
        TranslationUnitBlockAppend(unit, bb);

    return bb;
}

JBlock *argon::lang::compiler::TranslationUnitJBNew(TranslationUnit *unit, String *label) {
    JBlock *block = unit->jstack;
    BasicBlock *begin = unit->bb.cur;

    for (; block != nullptr; block = block->prev) {
        if (StringCompare(block->label, label) == 0 && block->nested == unit->symt->nested)
            break;
    }

    if (block == nullptr) {
        if (unit->bb.cur->i_size > 0) {
            if ((begin = TranslationUnitBlockNew(unit)) == nullptr)
                return nullptr;
        }

        if ((block = JBlockNew(unit->jstack, label, unit->symt->nested)) != nullptr) {
            block->start = begin;
            unit->jstack = block;
        }
    }

    return block;
}

JBlock *argon::lang::compiler::TranslationUnitJBNewLoop(TranslationUnit *unit, BasicBlock *begin, BasicBlock *end) {
    String *label = nullptr;
    JBlock *block;

    if (unit->jstack != nullptr) {
        if (unit->jstack->start->i_size == 0)
            label = unit->jstack->label;
    }

    if ((block = JBlockNew(unit->jstack, label, unit->symt->nested)) != nullptr) {
        block->start = begin;
        block->end = end;
        block->loop = true;
        unit->jstack = block;
    }

    return block;
}

JBlock *argon::lang::compiler::TranslationUnitJBFindLoop(TranslationUnit *unit, String *label) {
    JBlock *block = unit->jstack;

    for (; block != nullptr; block = block->prev) {
        if (label != nullptr) {
            if (StringCompare(block->label, label) != 0 || !block->loop)
                continue;
        }

        if (block->loop)
            break;
    }

    return block;
}

void argon::lang::compiler::TranslationUnitBlockAppend(TranslationUnit *unit, BasicBlock *block) {
    if (unit->bb.start == nullptr) {
        unit->bb.start = block;
        unit->bb.cur = block;
    } else {
        unit->bb.cur->next = block;
        unit->bb.cur = block;
    }
}

void argon::lang::compiler::TranslationUnitDecStack(TranslationUnit *unit, unsigned short size) {
    unit->stack.current -= size;
    assert(unit->stack.current < 0x00FFFFFF);
}

void argon::lang::compiler::TranslationUnitIncStack(TranslationUnit *unit, unsigned short size) {
    unit->stack.current += size;
    if (unit->stack.current > unit->stack.required)
        unit->stack.required = unit->stack.current;
}

void argon::lang::compiler::TranslationUnitJBPop(TranslationUnit *unit, JBlock *block) {
    JBlock *tmp = unit->jstack;

    if (tmp == block) {
        unit->jstack = JBlockDel(tmp);
        return;
    }

    for (JBlock *cur = unit->jstack->prev; cur != nullptr; cur = tmp) {
        if (cur == block) {
            tmp->prev = JBlockDel(cur);
            break;
        }
        tmp = cur;
    }
}
