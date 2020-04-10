// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "nil.h"

using namespace argon::object;

bool nil_equal(ArObject *self, ArObject *other) {
    return self->type == other->type;
}

size_t nil_hash(ArObject *obj) {
    return 0;
}

bool nil_istrue(Nil *self) {
    return false;
}

const TypeInfo type_nil_ = {
        (const unsigned char *) "nil",
        sizeof(Nil),
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) nil_istrue,
        nil_equal,
        nil_hash,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Nil *NilNew() noexcept {
    auto nil = (Nil *) argon::memory::Alloc(sizeof(Nil));
    assert(nil != nullptr);
    nil->strong_or_ref = 1;
    nil->type = &type_nil_;
    return nil;
}

Nil *argon::object::NilVal = NilNew();

