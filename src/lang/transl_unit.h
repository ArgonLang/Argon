// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_TRANSL_UNIT_H_
#define ARGON_LANG_TRANSL_UNIT_H_

#include <string>
#include <object/datatype/list.h>
#include <object/datatype/map.h>

#include "basicblock.h"
#include "symtable.h"

namespace argon::lang {

    enum class TUScope {
        FUNCTION,
        MODULE,
        STRUCT,
        TRAIT
    };

    class LoopMeta {
    public:
        const std::string *name;
        BasicBlock *begin = nullptr;
        BasicBlock *end = nullptr;

        LoopMeta *prev = nullptr;

        LoopMeta(const std::string *name) : name(name) {}
    };

    class TranslationUnit {
        void Dfs(BasicBlock *start);

    public:
        /* Name of translation unit (function/module/struct/trait) */
        const std::string name;

        /* Symbol table */
        SymTable symt;

        /* Pointer to prev translation unit */
        TranslationUnit *prev = nullptr;

        /* Local statics map */
        argon::object::Map *statics_map = nullptr;

        /* Static resources */
        argon::object::List *statics = nullptr;

        /* External variables (global scope) */
        argon::object::List *names = nullptr;

        /* Local variables (function/cycle scope) */
        argon::object::List *locals = nullptr;

        /* Closure */
        argon::object::List *enclosed = nullptr;

        LoopMeta *lstack = nullptr;

        struct {
            BasicBlock *list = nullptr;
            BasicBlock *start = nullptr;
            BasicBlock *current = nullptr;

            BasicBlock *flow_head = nullptr;
            BasicBlock *flow_tail = nullptr;
        } bb;

        unsigned int instr_sz = 0;

        struct {
            unsigned int required = 0;
            unsigned int current = 0;
        } stack;

        const TUScope scope;

        explicit TranslationUnit(std::string name, TUScope scope);

        ~TranslationUnit();

        BasicBlock *BlockNew();

        BasicBlock *BlockAsNextNew();

        LoopMeta *LoopBegin(const std::string &loop_name);

        LoopMeta *LoopGet(const std::string &loop_name);

        void LoopEnd();

        void BlockAsNext(BasicBlock *block);

        void IncStack();

        void IncStack(unsigned short size);

        void DecStack();

        void DecStack(unsigned short size);

        void Dfs();
    };

} // namespace argon::lang

#endif // !ARGON_LANG_TRANSL_UNIT_H_
