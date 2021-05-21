// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "string.h"

#include "bool.h"
#include "nil.h"

using namespace argon::object;

ArObject *nil_compare(Nil *self, ArObject *other, CompareMode mode) {
    if (!AR_TYPEOF(self, type_nil_) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArSize nil_hash(ArObject *self) {
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
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) nil_compare,
        nil_is_true,
        nil_hash,
        nil_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Nil NilDef{{RefCount(RCType::STATIC), &type_nil_}};

Nil *argon::object::NilVal = &NilDef;
