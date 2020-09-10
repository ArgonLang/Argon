// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <lang/compiler_exception.h>
#include "transl_unit.h"

using namespace argon::object;
using namespace argon::lang;

TranslationUnit::TranslationUnit(TUScope scope) : scope(scope) {
    if ((this->statics_map = MapNew()) == nullptr)
        goto error;
    if ((this->statics = ListNew()) == nullptr)
        goto error;
    if ((this->names = ListNew()) == nullptr)
        goto error;
    if ((this->locals = ListNew()) == nullptr)
        goto error;
    if ((this->enclosed = ListNew()) == nullptr)
        goto error;

    return;

    error:
    Release(this->statics_map);
    Release(this->statics);
    Release(this->names);
    Release(this->locals);
    Release(this->enclosed);
    throw MemoryException("");
}

TranslationUnit::~TranslationUnit() {
    for (BasicBlock *cursor = this->bb.list, *nxt; cursor != nullptr; cursor = nxt) {
        nxt = cursor->link_next;
        argon::memory::FreeObject(cursor);
    }

    Release(this->statics_map);
    Release(this->statics);
    Release(this->names);
    Release(this->locals);
    Release(this->enclosed);
}

BasicBlock *TranslationUnit::BlockNew() {
    auto block = argon::memory::AllocObject<BasicBlock>();

    if (block != nullptr) {
        // Append to block list
        block->link_next = this->bb.list;
        this->bb.list = block;

        // If it's the first block, set it as start
        if (this->bb.start == nullptr)
            this->bb.start = block;

        return block;
    }

    throw MemoryException("");
}

BasicBlock *TranslationUnit::BlockAsNextNew() {
    BasicBlock *bn = this->BlockNew();
    BasicBlock *bp = this->bb.current;

    if (this->bb.current != nullptr)
        this->bb.current->flow.next = bn;
    this->bb.current = bn;

    return bp;
}

void TranslationUnit::BlockAsNext(BasicBlock *block) {
    this->bb.current->flow.next = block;
    this->bb.current = block;
}

void TranslationUnit::IncStack() {
    this->IncStack(1);
}

void TranslationUnit::IncStack(unsigned short size) {
    this->stack.current++;
    if (this->stack.current > this->stack.required)
        this->stack.required = this->stack.current;
}

void TranslationUnit::DecStack() {
    this->DecStack(1);
}

void TranslationUnit::DecStack(unsigned short size) {
    this->stack.current -= size;
    assert(this->stack.current < 0x00FFFFFF);
}
