// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "integer.h"
#include "string.h"

#include "bool.h"

#define FALSE_AS_INT    0
#define TRUE_AS_INT     1

using namespace argon::object;

ArObject *bool_as_integer(Bool *self) {
    return IntegerNew(self->value ? TRUE_AS_INT : FALSE_AS_INT);
}

ArSSize bool_as_index(Bool *self) {
    return self->value ? TRUE_AS_INT : FALSE_AS_INT;
}

NumberSlots bool_nslots = {
        (UnaryOp) bool_as_integer,
        (ArSizeUnaryOp) bool_as_index
};

bool bool_equal(ArObject *self, ArObject *other) {
    return (other->type == self->type) && self == other;
}

size_t bool_hash(ArObject *obj) {
    return ((Bool *) obj)->value ? TRUE_AS_INT : FALSE_AS_INT;
}

bool bool_is_true(Bool *self) {
    return self->value;
}

ArObject *bool_str(Bool *self) {
    return StringIntern(self->value ? "true" : "false");
}

const TypeInfo argon::object::type_bool_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "bool",
        sizeof(Bool),
        nullptr,
        &bool_nslots,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) bool_is_true,
        bool_equal,
        nullptr,
        bool_hash,
        (UnaryOp) bool_str,
        nullptr,
        nullptr,
        nullptr
};

#define BOOL_TYPE(sname, export_name, value)                  \
Bool sname {{RefCount(RCType::STATIC), &type_bool_}, value};  \
Bool *export_name = &sname

BOOL_TYPE(BoolTrue, argon::object::True, true);
BOOL_TYPE(BoolFalse, argon::object::False, false);

#undef FALSE_AS_INT
#undef TRUE_AS_INT
#undef BOOL_TYPE