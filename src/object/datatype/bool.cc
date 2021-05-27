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

ARGON_FUNCTION5(bool_, new, "Creates a new bool from object."
                            ""
                            "- Parameter obj: obj to convert."
                            "- Returns: true or false.", 1, false) {
    return BoolToArBool(IsTrue(*argv));
}

const NativeFunc bool_methods[] = {
        bool_new_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots bool_obj{
        bool_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

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

    ARGON_RICH_COMPARE_CASES(l, r, mode);
}

ArSize bool_hash(ArObject *obj) {
    return ((Bool *) obj)->value ? TRUE_AS_INT : FALSE_AS_INT;
}

ArObject *bool_str(Bool *self) {
    return StringIntern(self->value ? "true" : "false");
}

const TypeInfo BoolType = {
        TYPEINFO_STATIC_INIT,
        "bool",
        nullptr,
        sizeof(Bool),
        TypeInfoFlags::BASE,
        nullptr,
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
        &bool_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_bool_ = &BoolType;

#define BOOL_TYPE(sname, export_name, value)                \
Bool sname {{RefCount(RCType::STATIC), type_bool_}, value}; \
Bool *export_name = &sname

BOOL_TYPE(BoolTrue, argon::object::True, true);
BOOL_TYPE(BoolFalse, argon::object::False, false);

#undef FALSE_AS_INT
#undef TRUE_AS_INT
#undef BOOL_TYPE