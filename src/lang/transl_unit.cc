// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "transl_unit.h"

using namespace argon::object;
using namespace argon::lang;

TranslationUnit::TranslationUnit(std::string name, TUScope scope) : name(std::move(name)), scope(scope) {
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
    throw std::bad_alloc();
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

    throw std::bad_alloc();
}

BasicBlock *TranslationUnit::BlockAsNextNew() {
    BasicBlock *bn = this->BlockNew();
    BasicBlock *bp = this->bb.current;

    if (this->bb.current != nullptr)
        this->bb.current->flow.next = bn;
    this->bb.current = bn;

    return bp;
}

LoopMeta *TranslationUnit::LoopBegin(const std::string &loop_name) {
    auto meta = argon::memory::AllocObject<LoopMeta>(&loop_name);

    meta->prev = this->lstack;
    this->lstack = meta;

    meta->begin = this->BlockNew();
    meta->end = this->BlockNew();

    this->BlockAsNext(meta->begin);

    return meta;
}

LoopMeta *TranslationUnit::LoopGet(const std::string &loop_name) {
    if (loop_name.empty())
        return this->lstack;

    for (LoopMeta *cu = this->lstack; cu != nullptr; cu = cu->prev) {
        if (*cu->name == loop_name)
            return cu;
    }

    return nullptr;
}

void TranslationUnit::LoopEnd() {
    auto meta = this->lstack;

    if (meta != nullptr) {
        this->lstack = meta->prev;
        this->BlockAsNext(meta->end);

        argon::memory::FreeObject(meta);
    }
}

void TranslationUnit::BlockAsNext(BasicBlock *block) {
    this->bb.current->flow.next = block;
    this->bb.current = block;
}

void TranslationUnit::IncStack() {
    this->IncStack(1);
}

void TranslationUnit::IncStack(unsigned short size) {
    this->stack.current += size;
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

void TranslationUnit::Dfs() {
    this->Dfs(this->bb.start);
}

void TranslationUnit::Dfs(BasicBlock *start) {
    start->visited = true;
    start->instr_sz_start = this->instr_sz;
    this->instr_sz += start->instr_sz;

    // APPEND
    if (this->bb.flow_head == nullptr) {
        this->bb.flow_head = start;
        this->bb.flow_tail = start;
    } else {
        this->bb.flow_tail->block_next = start;
        this->bb.flow_tail = start;
    }

    if (start->flow.next != nullptr && !start->flow.next->visited)
        this->Dfs(start->flow.next);

    if (start->flow.jump != nullptr && !start->flow.jump->visited)
        this->Dfs(start->flow.jump);
}
