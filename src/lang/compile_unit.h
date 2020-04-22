// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILE_UNIT_H_
#define ARGON_LANG_COMPILE_UNIT_H_

#include "compiler_exception.h"

namespace lang {
    enum class CUScope {
        MODULE,
        FUNCTION
    };

    struct CompileUnit {
        SymTUptr symt;
        argon::object::Map *statics_map;

        argon::object::List *statics;
        argon::object::List *names;
        argon::object::List *locals;
        argon::object::List *deref;

        std::vector<BasicBlock *> bb_splist;

        BasicBlock *bb_start = nullptr;
        BasicBlock *bb_list = nullptr;
        BasicBlock *bb_curr = nullptr;

        CompileUnit *prev = nullptr;

        unsigned int instr_sz = 0;
        unsigned int stack_sz = 0;
        unsigned int stack_cu_sz = 0;

        const CUScope scope;

        explicit CompileUnit(CUScope scope) : scope(scope) {
            if ((this->statics_map = argon::object::MapNew()) == nullptr)
                throw MemoryException("CompileUnit: statics_map");

            if ((this->statics = argon::object::ListNew()) == nullptr) {
                argon::object::Release(this->statics_map);
                throw MemoryException("CompileUnit: statics");
            }

            if ((this->names = argon::object::ListNew()) == nullptr) {
                argon::object::Release(this->statics_map);
                argon::object::Release(this->statics);
                throw MemoryException("CompileUnit: names");
            }

            if ((this->locals = argon::object::ListNew()) == nullptr) {
                argon::object::Release(this->statics_map);
                argon::object::Release(this->statics);
                argon::object::Release(this->names);
                throw MemoryException("CompileUnit: locals");
            }

            if ((this->deref = argon::object::ListNew()) == nullptr) {
                argon::object::Release(this->statics_map);
                argon::object::Release(this->statics);
                argon::object::Release(this->names);
                argon::object::Release(this->locals);
                throw MemoryException("CompileUnit: deref");
            }
        }

        ~CompileUnit() {
            for (BasicBlock *cursor = this->bb_list, *nxt; cursor != nullptr; cursor = nxt) {
                nxt = cursor->link_next;
                delete (cursor);
            }
            argon::object::Release(this->statics_map);
            argon::object::Release(this->statics);
            argon::object::Release(this->names);
            argon::object::Release(this->locals);
            argon::object::Release(this->deref);
        }
    };
} // namespace lang

#endif // !ARGON_LANG_COMPILE_UNIT_H_
