// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/arobject.h>
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

bool code_istrue(ArObject *self) {
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
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "code",
        sizeof(Code),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        code_istrue,
        code_equal,
        nullptr,
        code_hash,
        nullptr,
        nullptr,
        nullptr,
        code_cleanup
};


Code *argon::object::CodeNew(const unsigned char *instr,
                             unsigned int instr_sz,
                             unsigned int stack_sz,
                             List *statics,
                             List *names,
                             List *locals,
                             List *enclosed) {
    auto code = ArObjectNew<Code>(RCType::INLINE, &type_code_);

    if (code != nullptr) {
        code->instr = instr;
        code->instr_sz = instr_sz;
        code->stack_sz = stack_sz;

        if ((code->statics = TupleNew(statics)) == nullptr)
            Release(code);
        if ((code->names = TupleNew(names)) == nullptr)
            Release(code);
        if ((code->locals = TupleNew(locals)) == nullptr)
            Release(code);
        if ((code->enclosed = TupleNew(enclosed)) == nullptr)
            Release(code);
    }

    return code;
}
