// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "hash_magic.h"
#include "code.h"

using namespace argon::object;

Code::Code(const unsigned char *instr, unsigned int instr_sz, unsigned int stack_sz, argon::object::List *statics,
           argon::object::List *names, argon::object::List *locals) : Object(&type_code_),
                                                                      instr(instr),
                                                                      instr_sz(instr_sz),
                                                                      stack_sz(stack_sz) {
    this->statics = NewObject<Tuple>(statics);
    this->names = NewObject<Tuple>(names);
    this->locals = NewObject<Tuple>(locals);
}

Code::~Code() {
    argon::memory::Free((unsigned char *) this->instr);
    ReleaseObject(this->statics);
    ReleaseObject(this->names);
    ReleaseObject(this->locals);
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
