// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "hash_magic.h"
#include "code.h"

using namespace argon::object;

Code::Code(size_t instr_sz) : Object(&type_code_), instr_sz(instr_sz) {
    this->instr = (unsigned char *) argon::memory::Alloc(instr_sz);
}

Code::~Code() {
    argon::memory::Free(this->instr);
    ReleaseObject(this->statics);
    ReleaseObject(this->names);
}

bool Code::EqualTo(const Object *other) {
    if (this != other) {
        if (this->type != other->type || this->instr_sz != ((Code *) other)->instr_sz)
            return false;

        for (size_t i = 0; i < this->instr_sz; i++) {
            if (this->instr[i] != ((Code *) other)->instr[i])
                return false;
        }
    }

    return true;
}

size_t Code::Hash() {
    if (this->hash_ == 0)
        this->hash_ = HashBytes(this->instr, this->instr_sz);
    return this->hash_;
}
