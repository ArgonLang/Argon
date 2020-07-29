// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <memory/memory.h>

#include "hash_magic.h"
#include "code.h"

using namespace argon::memory;
using namespace argon::object;

bool code_equal(ArObject *self, ArObject *other) {
    if (self != other) {
        if (self->type != other->type || ((Code *) self)->instr_sz != ((Code *) other)->instr_sz)
            return false;

        for (size_t i = 0; i < ((Code *) self)->instr_sz; i++) {
            if (((Code *) self)->instr[i] != ((Code *) other)->instr[i])
                return false;
        }
    }

    return true;
}

size_t code_hash(ArObject *obj) {
    auto code = (Code *) obj;
    if (code->hash == 0)
        code->hash = HashBytes(code->instr, code->instr_sz);
    return code->hash;
}

bool code_istrue(Code *self) {
    return true;
}

void code_cleanup(ArObject *obj) {
    auto code = (Code *) obj;
    Free((void *) code->instr);
    Release(code->statics);
    Release(code->names);
    Release(code->locals);
}

const TypeInfo argon::object::type_code_ = {
        (const unsigned char *) "code",
        sizeof(Code),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) code_istrue,
        code_equal,
        nullptr,
        code_hash,
        nullptr,
        nullptr,
        code_cleanup
};


Code *argon::object::CodeNew(const unsigned char *instr,
                             unsigned int instr_sz,
                             unsigned int stack_sz,
                             argon::object::List *statics,
                             argon::object::List *names,
                             argon::object::List *locals,
                             argon::object::List *deref) {
    auto code = ArObjectNew<Code>(RCType::INLINE, &type_code_);
    code->type = &type_code_;
    assert(code != nullptr);

    code->instr = instr;
    code->instr_sz = instr_sz;
    code->stack_sz = stack_sz;

    code->statics = TupleNew(statics);
    assert(code->statics != nullptr);
    code->names = TupleNew(names);
    assert(code->names != nullptr);
    code->locals = TupleNew(locals);
    assert(code->locals != nullptr);
    code->deref = TupleNew(deref);
    assert(code->deref != nullptr);

    return code;
}
