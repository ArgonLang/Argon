// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_TRANSLATION_UNIT_H_
#define ARGON_LANG_COMPILER_TRANSLATION_UNIT_H_

#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/string.h>

#include "basicblock.h"

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

        /* Name of translation unit */
        argon::object::String *name;

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

        struct {
            BasicBlock *start;
            BasicBlock *cur;
        } bb;

        TUScope scope;

        struct {
            unsigned int required;
            unsigned int current;
        } stack;
    };

    TranslationUnit *TranslationUnitNew(argon::object::String *name, TUScope scope);

    TranslationUnit *TranslationUnitDel(TranslationUnit *unit);

    BasicBlock *TranslationUnitBlockNew(TranslationUnit *unit);

    void TranslationUnitDecStack(TranslationUnit *unit, unsigned short size);

    void TranslationUnitIncStack(TranslationUnit *unit, unsigned short size);
}

#endif // !ARGON_LANG_COMPILER_TRANSLATION_UNIT_H_
