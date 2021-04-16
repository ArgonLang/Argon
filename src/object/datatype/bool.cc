// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "integer.h"
#include "string.h"

#include "error.h"
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

ArObject *bool_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("bool", count, 0, 1))
        return nullptr;

    if (count == 1)
        return BoolToArBool(IsTrue(*args));

    return False;
}

bool bool_is_true(Bool *self) {
    return self->value;
}

ArObject *bool_compare(Bool *self, ArObject *other, CompareMode mode) {
    IntegerUnderlying l = self->value;
    IntegerUnderlying r;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (AR_TYPEOF(other, type_bool_))
        r = ((Bool *) other)->value;
    else if (AR_TYPEOF(other, type_integer_))
        r = ((Integer *) other)->integer;
    else
        return nullptr;

    ARGON_RICH_COMPARE_CASES(l, r, mode)
}

size_t bool_hash(ArObject *obj) {
    return ((Bool *) obj)->value ? TRUE_AS_INT : FALSE_AS_INT;
}

ArObject *bool_str(Bool *self) {
    return StringIntern(self->value ? "true" : "false");
}

const TypeInfo argon::object::type_bool_ = {
        TYPEINFO_STATIC_INIT,
        "bool",
        nullptr,
        sizeof(Bool),
        TypeInfoFlags::BASE,
        bool_ctor,
        nullptr,
        nullptr,
        (CompareOp) bool_compare,
        (BoolUnaryOp) bool_is_true,
        bool_hash,
        (UnaryOp) bool_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &bool_nslots,
        nullptr,
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

#undef FALSE_AS_INT
#undef TRUE_AS_INT
#undef BOOL_TYPE