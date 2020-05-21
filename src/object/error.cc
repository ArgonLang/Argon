// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "error.h"

using namespace argon::object;

const TypeInfo type_not_implemented_ = {
        (const unsigned char *) "NotImplemented",
        sizeof(NotImplemented),
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

NotImplemented *NotImplementedNew() noexcept {
    static NotImplemented ni;
    ni.ref_count =  ARGON_OBJECT_REFCOUNT_STATIC;
    ni.type = &type_not_implemented_;
    return &ni;
}

ArObject *argon::object::NotImpl = NotImplementedNew();


