// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_TRANSLATION_UNIT_H_
#define ARGON_LANG_COMPILER_TRANSLATION_UNIT_H_

#include <object/datatype/code.h>
#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/string.h>

#include "basicblock.h"
#include "symtable.h"

namespace argon::lang::compiler {

    enum class TUScope {
        FUNCTION,
        MODULE,
        STRUCT,
        TRAIT
    };

    struct TranslationUnit {
        /* Pointer to prev translation unit */
        TranslationUnit *prev;

        /* Pointer to current scope SymbolTable */
        SymbolTable *symt;

        /* Pointer to the symbol that describe this scope */
        Symbol *info;

        /* Name of translation unit */
        argon::object::String *name;

        /* Qualified name of translation unit */
        argon::object::String *qname;

        /* Local statics map */
        argon::object::Map *statics_map;

        /* Static resources */
        argon::object::List *statics;

        /* External variables (global scope) */
        argon::object::List *names;

        /* Local variables (function/cycle scope) */
        argon::object::List *locals;

        /* Closure */
        argon::object::List *enclosed;

        JBlock *jstack;

        struct {
            BasicBlock *start;
            BasicBlock *cur;
        } bb;

        TUScope scope;

        struct {
            unsigned int required;
            unsigned int current;
        } stack;

        unsigned short anon_count;
    };

    inline bool TranslationUnitEnterSub(TranslationUnit *unit) {
        return SymbolTableEnterSub(&unit->symt);
    }

    bool TranslationUnitIsFreeVar(TranslationUnit *unit, argon::object::String *name);

    object::Code *TranslationUnitAssemble(TranslationUnit *unit);

    TranslationUnit *TranslationUnitNew(TranslationUnit *prev, argon::object::String *name, TUScope scope,
                                        SymbolTable *symt, Symbol *symbol);

    TranslationUnit *TranslationUnitDel(TranslationUnit *unit);

    BasicBlock *TranslationUnitBlockNew(TranslationUnit *unit);

    JBlock *TranslationUnitJBNew(TranslationUnit *unit, argon::object::String *label);

    inline JBlock *TranslationUnitJBNew(TranslationUnit *unit, argon::object::String *label, BasicBlock *end) {
        auto *jb = TranslationUnitJBNew(unit, label);
        if (jb != nullptr)
            jb->end = end;
        return jb;
    }

    JBlock *TranslationUnitJBNewLoop(TranslationUnit *unit, BasicBlock *begin, BasicBlock *end);

    JBlock *TranslationUnitJBFindLoop(TranslationUnit *unit, argon::object::String *label);

    inline void TranslationUnitExitSub(TranslationUnit *unit) {
        SymbolTableExitSub(&unit->symt);
    }

    void TranslationUnitBlockAppend(TranslationUnit *unit, BasicBlock *block);

    void TranslationUnitDecStack(TranslationUnit *unit, unsigned short size);

    void TranslationUnitIncStack(TranslationUnit *unit, unsigned short size);

    void TranslationUnitJBPop(TranslationUnit *unit, JBlock *block);
}

#endif // !ARGON_LANG_COMPILER_TRANSLATION_UNIT_H_
