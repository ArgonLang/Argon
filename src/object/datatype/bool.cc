// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"

using namespace argon::object;

bool bool_equal(ArObject *self, ArObject *other) {
    return (other->type == self->type) && ((Bool *) other)->value;
}

size_t bool_hash(ArObject *obj) {
    return ((Bool *) obj)->value ? 1 : 0;
}

bool bool_istrue(Bool *self) {
    return self->value;
}

const TypeInfo type_bool_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "bool",
        sizeof(Bool),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) bool_istrue,
        bool_equal,
        nullptr,
        bool_hash,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

#define BOOL_TYPE(sname, export_name, value)                  \
Bool sname {{RefCount(RCType::STATIC), &type_bool_}, value};  \
Bool *export_name = &sname

BOOL_TYPE(BoolTrue, argon::object::True, true);
BOOL_TYPE(BoolFalse, argon::object::False, false);

#undef BOOL_TYPE