// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"

using namespace argon::object;

bool nil_equal(ArObject *self, ArObject *other) {
    return self->type == other->type;
}

size_t nil_hash(ArObject *obj) {
    return 0;
}

bool nil_istrue(ArObject *self) {
    return false;
}

const TypeInfo argon::object::type_nil_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "nil",
        sizeof(Nil),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nil_istrue,
        nil_equal,
        nullptr,
        nil_hash,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Nil NilDef{{RefCount(RCType::STATIC), &type_nil_}};

Nil *argon::object::NilVal = &NilDef;
