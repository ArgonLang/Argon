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

bool bool_is_true(Bool *self) {
    return self->value;
}

bool bool_equal(ArObject *self, ArObject *other) {
    return self == other;
}

ArObject *bool_compare(Bool *self, ArObject *other, CompareMode mode) {
    IntegerUnderlayer l = self->value;
    IntegerUnderlayer r;

    if (AR_TYPEOF(other, type_bool_))
        r = ((Bool *) other)->value;
    else if (AR_TYPEOF(other, type_integer_))
        r = ((Integer *) other)->integer;
    else
        return nullptr;

    switch (mode) {
        case CompareMode::GE:
            return BoolToArBool(l > r);
        case CompareMode::GEQ:
            return BoolToArBool(l >= r);
        case CompareMode::LE:
            return BoolToArBool(l < r);
        case CompareMode::LEQ:
            return BoolToArBool(l <= r);
        default:
            assert(false);
    }
}

size_t bool_hash(ArObject *obj) {
    return ((Bool *) obj)->value ? TRUE_AS_INT : FALSE_AS_INT;
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
        (CompareOp) bool_compare,
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