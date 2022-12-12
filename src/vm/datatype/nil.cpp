// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"

#include "nil.h"

using namespace argon::vm::datatype;

ArObject *nil_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (!AR_TYPEOF(self, type_nil_) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArObject *nil_str(const ArObject *) {
    return (ArObject *) StringNew("nil");
}

bool nil_is_true(const ArObject *) {
    return false;
}

ArSize nil_hash(const ArObject *) {
    return 0;
}

TypeInfo NilType = {
        AROBJ_HEAD_INIT_TYPE,
        "Nil",
        nullptr,
        nullptr,
        sizeof(NilBase),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nil_hash,
        nil_is_true,
        nil_compare,
        nullptr,
        (UnaryOp) nil_str,
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
const TypeInfo *argon::vm::datatype::type_nil_ = &NilType;

constexpr NilBase NilDef{
        AROBJ_HEAD_INIT(&NilType)
};

NilBase *argon::vm::datatype::Nil = (NilBase *) &NilDef;