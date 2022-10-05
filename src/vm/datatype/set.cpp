// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "set.h"

using namespace argon::vm::datatype;

TypeInfo SetType = {
        AROBJ_HEAD_INIT_TYPE,
        "Set",
        nullptr,
        nullptr,
        sizeof(Set),
        TypeInfoFlags::BASE,
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
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_set_ = &SetType;

