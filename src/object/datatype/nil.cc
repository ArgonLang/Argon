// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "string.h"

#include "nil.h"

using namespace argon::object;

bool nil_equal(ArObject *self, ArObject *other) {
    return self->type == other->type;
}

size_t nil_hash(ArObject *self) {
    return 0;
}

bool nil_is_true(ArObject *self) {
    return false;
}

ArObject *nil_str(ArObject *self) {
    return StringIntern("nil");
}

const TypeInfo argon::object::type_nil_ = {
        TYPEINFO_STATIC_INIT,
        "nil",
        nullptr,
        sizeof(Nil),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nil_equal,
        nil_is_true,
        nil_hash,
        nil_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Nil NilDef{{RefCount(RCType::STATIC), &type_nil_}};

Nil *argon::object::NilVal = &NilDef;
