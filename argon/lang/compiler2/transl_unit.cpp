// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/exception.h>

#include <argon/lang/compiler2/optimizer/optimizer.h>

#include <argon/lang/compiler2/transl_unit.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;

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

BasicBlock *TranslationUnit::BlockAppend(BasicBlock *block) {
    this->bbb.Append(block);
    return block;
}

BasicBlock *TranslationUnit::BlockNew() {
    auto *block = this->bbb.BlockNewAppend();
    if (block == nullptr)
        throw DatatypeException();

    return block;
}

Code *TranslationUnit::Assemble(String *docs, OptimizationLevel level) {
    CodeOptimizer optim(this, level);
    Code *code;

    // Instructions buffer
    unsigned char *instr_buf;
    unsigned char *instr_cur;

    // Line -> OpCode mapping
    unsigned char *line_buf;
    unsigned char *line_cur;

    unsigned int instr_sz;
    unsigned int line_sz;

    unsigned int last_lineno = 0;
    ArSize last_opoff = 0;

    optim.optimize();

    this->ComputeAssemblyLength(&instr_sz, &line_sz);

    if (instr_sz == 0) {
        if ((code = CodeNew(this->statics, this->names, this->locals, this->enclosed)) == nullptr)
            throw DatatypeException();

        return code->SetInfo(this->name, this->qname, docs);
    }

    if ((instr_buf = (unsigned char *) vm::memory::Alloc(instr_sz)) == nullptr)
        throw DatatypeException();

    line_buf = nullptr;
    if (line_sz > 0) {
        line_buf = (unsigned char *) vm::memory::Alloc(line_sz);
        if (line_buf == nullptr) {
            vm::memory::Free(instr_buf);

            throw DatatypeException();
        }
    }

    instr_cur = instr_buf;
    line_cur = line_buf;

    for (auto *cursor = this->bbb.begin; cursor != nullptr; cursor = cursor->next) {
        for (auto *instr = cursor->instr.head; instr != nullptr; instr = instr->next) {
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

            // Line -> OpCode mapping
            // 2 Byte entry: OpCode Offset, Line Offset
            // If Line Offset > 127:
            // e.g. OpCode offset: 33, Line offset: 241
            //      (33, 127), (0, 114)
            // If Line Offset < 128:
            // e.g. OpCode offset: 12, Line offset: -300
            //      (12, -128), (0, -128), (0, -44)

            if (line_sz > 0) {
                auto ldiff = (int) instr->lineno - (int) last_lineno;

                if (line_buf == nullptr || instr->lineno == 0 || ldiff == 0)
                    continue;

                *line_cur++ = (unsigned char) ((instr_cur - instr_buf) - last_opoff);

                while (ldiff > 0) {
                    *line_cur++ = ldiff > 127 ? 127 : (char) ldiff;
                    ldiff -= ldiff > 127 ? 127 : ldiff;

                    if (ldiff > 0)
                        *line_cur++ = 0; // No OpCode offset
                }

                while (ldiff < 0) {
                    *line_cur++ = ldiff < -128 ? -128 : (char) ldiff;
                    ldiff += 128;

                    if (ldiff < 0)
                        *line_cur++ = 0; // No OpCode offset
                }

                last_opoff = instr_cur - instr_buf;
                last_lineno = instr->lineno;
            }
        }
    }

    if ((code = CodeNew(this->statics, this->names, this->locals, this->enclosed)) == nullptr) {
        vm::memory::Free(instr_buf);
        vm::memory::Free(line_buf);

        throw DatatypeException();
    }

    return code
            ->SetInfo(this->name, this->qname, docs)
            ->SetBytecode(instr_buf, instr_sz, this->stack.required, this->sync_stack.required)
            ->SetTracingInfo(line_buf, line_sz);
}

JBlock *TranslationUnit::JBFindLabel(const String *label, unsigned short &out_pops) const {
    out_pops = 0;

    for (auto *block = this->jblock; block != nullptr; block = block->prev) {
        out_pops += block->pops;

        if (label == nullptr && (block->type == JBlockType::LOOP || block->type == JBlockType::SWITCH))
            return block;

        if (block->type != JBlockType::LOOP)
            continue;

        if (block->label != nullptr && StringCompare(block->label, label) == 0)
            return block;
    }

    return nullptr;
}

JBlock *TranslationUnit::JBPush(String *label, BasicBlock *begin, BasicBlock *end, JBlockType type) {
    auto *j = JBlockNew(this->jblock, label, type);
    if (j == nullptr)
        throw DatatypeException();

    j->begin = begin;
    j->end = end;

    this->jblock = j;

    return j;
}

JBlock *TranslationUnit::JBPush(String *label, JBlockType type) {
    auto *j = JBlockNew(this->jblock, label, type);
    if (j == nullptr)
        throw DatatypeException();

    auto *begin = this->bbb.current;

    if (begin == nullptr || begin->size > 0) {
        begin = BasicBlockNew();
        if (begin == nullptr) {
            JBlockDel(j);

            throw DatatypeException();
        }

        this->bbb.Append(begin);
    }

    j->begin = begin;

    this->jblock = j;

    return j;
}

JBlock *TranslationUnit::JBPush(BasicBlock *begin, BasicBlock *end) {
    String *label = nullptr;

    if (this->jblock != nullptr && this->jblock->type == JBlockType::LABEL)
        label = this->jblock->label;

    return this->JBPush(label, begin, end, JBlockType::LOOP);
}

JBlock *TranslationUnit::JBPush(BasicBlock *begin, BasicBlock *end, unsigned short pops) {
    auto jb = this->JBPush(begin, end);

    jb->pops = pops;

    return jb;
}

bool TranslationUnit::CheckBlock(JBlockType expected) const {
    return this->jblock != nullptr && this->jblock->type == expected;
}

bool TranslationUnit::IsFreeVar(const String *id) const {
    // Look back in the TranslationUnits,
    // if a variable with the same name exists and is declared or free
    // in turn then this is a free variable
    SymbolT *sym;

    for (TranslationUnit *tu = this->prev; tu != nullptr; tu = tu->prev) {
        if (tu->symt->type == SymbolType::STRUCT || tu->symt->type == SymbolType::TRAIT)
            continue;

        if ((sym = tu->symt->SymbolLookup(id, false)) != nullptr) {
            // WARNING: sym->nested must be greater than 0,
            // otherwise this is a global variable.
            if (sym->type == SymbolType::VARIABLE
                && sym->nested > 0
                && (sym->declared || sym->free)) {
                Release(sym);

                return true;
            }

            Release(sym);
        }
    }

    return false;
}

void TranslationUnit::JBPop() {
    this->jblock = JBlockDel(this->jblock);
}

void TranslationUnit::Emit(vm::OpCode op, int arg, BasicBlock *dest, const scanner::Loc *loc) {
    unsigned int lineno = 0;

    if (loc != nullptr)
        lineno = (unsigned int) loc->start.line;

    if (this->bbb.AddInstr(dest, op, arg, lineno) == nullptr)
        throw DatatypeException();

    switch (op) {
        case vm::OpCode::CALL:
        case vm::OpCode::DFR:
        case vm::OpCode::INIT:
        case vm::OpCode::SPW:
            this->DecrementStack(arg & 0xFFFF);
            break;
        case vm::OpCode::DUP:
            this->IncrementStack(arg);
            break;
        case vm::OpCode::MKDT:
        case vm::OpCode::MKLT:
        case vm::OpCode::MKST:
        case vm::OpCode::MKSTRUCT:
        case vm::OpCode::MKTP:
        case vm::OpCode::MKTRAIT:
        case vm::OpCode::POPGT:
            this->DecrementStack(arg);
            break;
        default:
            break;
    }

    this->IncrementStack(vm::StackChange[(unsigned char) op]);
}

void TranslationUnit::ComputeAssemblyLength(unsigned int *instr_size, unsigned int *linfo_size) {
    unsigned int isz = 0;
    unsigned int lsz = 0;
    unsigned int last_lineno = 0;

    for (auto *cursor = this->bbb.begin; cursor != nullptr; cursor = cursor->next) {
        for (auto *instr = cursor->instr.head; instr != nullptr; instr = instr->next) {
            auto ldiff = (int) instr->lineno - (int) last_lineno;

            if (instr->lineno == 0 || ldiff == 0)
                continue;

            while (ldiff > 0) {
                ldiff = ldiff > 127 ? ldiff - 127 : 0;
                lsz += 2;
            }

            while (ldiff < 0) {
                ldiff += 128;
                lsz += 2;
            }

            last_lineno = instr->lineno;
        }

        cursor->offset = isz;
        isz += cursor->size;
    }

    if (instr_size != nullptr)
        *instr_size = isz;

    if (linfo_size != nullptr)
        *linfo_size = lsz;
}

void TranslationUnit::IncStaticUsage(int inc_index) {
    if (inc_index >= this->statics_usg_length) {
        auto *tmp = (int *) argon::vm::memory::Realloc(this->statics_usg_count, (this->statics_usg_length + 10) * sizeof(int));
        if (tmp == nullptr)
            throw DatatypeException();

        for (auto i = this->statics_usg_length; i < this->statics_usg_length + 10; i++)
            tmp[i] = 0;

        this->statics_usg_count = tmp;
        this->statics_usg_length += 10;
    }

    this->statics_usg_count[inc_index]++;
}

// *********************************************************************************************************************
// FUNCTIONS
// *********************************************************************************************************************

TranslationUnit *argon::lang::compiler2::TranslationUnitNew(TranslationUnit *prev, String *name, SymbolType type) {
    auto *symt = SymbolTableNew(prev != nullptr ? prev->symt : nullptr, name, type);
    if (symt == nullptr)
        return nullptr;

    auto *tu = (TranslationUnit *) argon::vm::memory::Calloc(sizeof(TranslationUnit));

    if (tu != nullptr) {
        if (symt->type == SymbolType::STRUCT || symt->type == SymbolType::TRAIT)
            vm::memory::MemoryCopy(tu, prev, sizeof(TranslationUnit));

        tu->prev = prev;

        tu->symt = symt;

        if (!MakeQName(prev, tu, name)) {
            argon::vm::memory::Free(tu);
            return nullptr;
        }

        if (symt->type != SymbolType::STRUCT && symt->type != SymbolType::TRAIT) {
            if ((tu->statics_map = DictNew()) == nullptr)
                goto ERROR;

            if ((tu->statics = ListNew()) == nullptr)
                goto ERROR;

            tu->statics_usg_length = kListInitialCapacity;
            if ((tu->statics_usg_count = (int *) vm::memory::Calloc(tu->statics_usg_length * sizeof(int))) == nullptr)
                goto ERROR;

            if ((tu->names = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->locals = ListNew()) == nullptr)
                goto ERROR;

            if ((tu->enclosed = ListNew()) == nullptr)
                goto ERROR;
        }
    }

    return tu;

    ERROR:
    Release(symt);

    Release(tu->name);
    Release(tu->qname);
    Release(tu->statics_map);
    Release(tu->statics);
    Release(tu->names);
    Release(tu->locals);

    argon::vm::memory::Free(tu->statics_usg_count);
    argon::vm::memory::Free(tu);

    return nullptr;
}

TranslationUnit *argon::lang::compiler2::TranslationUnitDel(TranslationUnit *unit) {
    if (unit == nullptr)
        return nullptr;

    auto *prev = unit->prev;

    if (unit->symt->type == SymbolType::STRUCT || unit->symt->type == SymbolType::TRAIT) {
        auto stack = unit->stack.required + prev->stack.current;

        if (prev->bbb.begin == nullptr)
            prev->bbb.begin = unit->bbb.begin;

        prev->bbb.current = unit->bbb.current;
        prev->statics_usg_count = unit->statics_usg_count;
        prev->statics_usg_length = unit->statics_usg_length;

        if (prev->stack.required < stack)
            prev->stack.required = stack;

        argon::vm::memory::Free(unit);

        return prev;
    }

    Release(unit->symt);
    Release(unit->name);
    Release(unit->qname);
    Release(unit->statics_map);
    Release(unit->statics);
    Release(unit->names);
    Release(unit->locals);
    Release(unit->enclosed);

    argon::vm::memory::Free(unit->statics_usg_count);

    // Free all BasicBlock
    BasicBlock *tmp = unit->bbb.begin;
    while ((tmp = BasicBlockDel(tmp)) != nullptr);

    // Free all JBlock
    JBlock *jb = unit->jblock;
    while ((jb = JBlockDel(jb)) != nullptr);

    argon::vm::memory::Free(unit);

    return prev;
}
