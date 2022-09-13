// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "code.h"

using namespace argon::vm::datatype;

const TypeInfo CodeType = {
        AROBJ_HEAD_INIT_TYPE,
        "Code",
        nullptr,
        nullptr,
        sizeof(Code),
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
const TypeInfo *argon::vm::datatype::type_code_ = &CodeType;
