// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "error.h"

using namespace argon::object;

#define ERROR_NEW_TYPE(type_name, name, base, obj_actions)      \
const TypeInfo type_##type_name##_ = {                          \
        (const unsigned char *) #name,                          \
        sizeof(base),                                           \
        nullptr,                                                \
        nullptr,                                                \
        nullptr,                                                \
        obj_actions,                                            \
        nullptr,                                                \
        nullptr,                                                \
        nullptr,                                                \
        nullptr,                                                \
        nullptr,                                                \
        nullptr,                                                \
        nullptr                                                 \
}

#define ERROR_STATIC_INIT(base, name, ptr_name, type, obj)  \
base name {                                                 \
        {RefCount(RCType::STATIC), &type},                  \
        obj                                                 \
};                                                          \
ArObject *ptr_name = &name

ArObject *error_getattr(Error *self, ArObject *key) {
    /*
    if (StringEq((String *) key, "err")) {
        IncRef(self->obj);
        return self->obj;
    }
     */
    return nullptr;
}

ObjectActions error_actions{
        (BinaryOp) error_getattr,
        nullptr
};

// Generic error that can encapsulate any ArObject
ERROR_NEW_TYPE(error, error, Error, &error_actions);

Error *argon::object::ErrorNew(ArObject *obj) {
    auto error = ArObjectNew<Error>(RCType::INLINE, &type_error_);

    if (error != nullptr)
        error->obj = obj;

    return error;
}

// ArithmeticError
ERROR_NEW_TYPE(zero_division_error, ZeroDivisionError, ErrorStr, nullptr);
ERROR_STATIC_INIT(ErrorStr, ZeroDivision, argon::object::ZeroDivisionError, type_zero_division_error_,
                  "divide by zero");

// RuntimeError
ERROR_NEW_TYPE(oo_memory, OutOfMemory, ErrorStr, nullptr);
ERROR_STATIC_INIT(ErrorStr, OutOfMemory, argon::object::OutOfMemoryError, type_oo_memory_, "out of memory");

ERROR_NEW_TYPE(not_impl, NotImplemented, ErrorStr, nullptr);
ERROR_STATIC_INIT(ErrorStr, NotImplemented, argon::object::NotImplementedError, type_not_impl_, "not implemented");

