// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"

using namespace argon::vm::datatype;

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
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_nil_ = &NilType;

constexpr NilBase NilDef{
        AROBJ_HEAD_INIT(&NilType)
};

NilBase *argon::vm::datatype::Nil = (NilBase *) &NilDef;