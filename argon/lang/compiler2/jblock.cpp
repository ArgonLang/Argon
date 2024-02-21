// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/compiler2/jblock.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;

JBlock *argon::lang::compiler2::JBlockNew(JBlock *prev, String *label, JBlockType type) {
    auto *j = (JBlock *) vm::memory::Calloc(sizeof(JBlock));

    if (j != nullptr) {
        j->prev = prev;

        j->label = IncRef(label);

        j->type = type;
    }

    return j;
}

JBlock *argon::lang::compiler2::JBlockDel(JBlock *block) {
    if (block == nullptr)
        return nullptr;

    auto *prev = block->prev;

    Release(block->label);

    vm::memory::Free(block);

    return prev;
}