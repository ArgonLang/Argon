// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/arobject.h>
#include "bool.h"
#include "hash_magic.h"
#include "string.h"
#include "code.h"

using namespace argon::memory;
using namespace argon::object;

bool code_is_true(ArObject *self) {
    return true;
}

bool code_equal(Code *self, ArObject *other) {
    if (self == other)
        return true;

    if (!AR_SAME_TYPE(self, other) || self->instr_sz != ((Code *) other)->instr_sz)
        return false;

    for (size_t i = 0; i < self->instr_sz; i++) {
        if (self->instr[i] != ((Code *) other)->instr[i])
            return false;
    }

    return true;
}

ArObject *code_compare(Code *self, ArObject *other, CompareMode mode) {
    bool equal = false;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self->instr_sz == ((Code *) other)->instr_sz) {
        equal = true;
        for (size_t i = 0; i < self->instr_sz; i++) {
            if (self->instr[i] != ((Code *) other)->instr[i]) {
                equal = false;
                break;
            }
        }
    }

    return BoolToArBool(equal);
}

size_t code_hash(Code *self) {
    if (self->hash == 0)
        self->hash = HashBytes(self->instr, self->instr_sz);

    return self->hash;
}

ArObject *code_str(ArObject *self) {
    return StringNewFormat("<code at %p>", self);
}

void code_cleanup(Code *self) {
    Release(self->statics);
    Release(self->names);
    Release(self->locals);
    Free((void *) self->instr);
}

const TypeInfo argon::object::type_code_ = {
        TYPEINFO_STATIC_INIT,
        "code",
        nullptr,
        sizeof(Code),
        nullptr,
        (VoidUnaryOp) code_cleanup,
        nullptr,
        (CompareOp) code_compare,
        (BoolBinOp) code_equal,
        code_is_true,
        (SizeTUnaryOp) code_hash,
        (UnaryOp) code_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
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
