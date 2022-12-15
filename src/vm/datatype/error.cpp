// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/runtime.h>

#include "atom.h"
#include "error.h"

using namespace argon::vm::datatype;

Error *argon::vm::datatype::error_div_by_zero = nullptr;
Error *argon::vm::datatype::error_oom = nullptr;
Error *argon::vm::datatype::error_err_oom = nullptr;
Error *argon::vm::datatype::error_while_error = nullptr;

TypeInfo ErrorType = {
        AROBJ_HEAD_INIT_TYPE,
        "Error",
        nullptr,
        nullptr,
        sizeof(Error),
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
const TypeInfo *argon::vm::datatype::type_error_ = &ErrorType;

bool argon::vm::datatype::ErrorInit() {
    error_oom = ErrorNew(kOOMError[0], kOOMError[1]);
    if (error_oom == nullptr)
        return false;

    error_err_oom = ErrorNew(kOOMError[0], kOOMError[2]);
    if (error_err_oom == nullptr) {
        Release(error_oom);
        return false;
    }

    error_div_by_zero = ErrorNew(kDivByZeroError[0], kDivByZeroError[1]);
    if (error_div_by_zero == nullptr) {
        Release(error_oom);
        Release(error_err_oom);
        return false;
    }

    error_while_error = ErrorNew(kErrorError[0], kErrorError[1]);
    if (error_while_error == nullptr) {
        Release(error_oom);
        Release(error_err_oom);
        Release(error_div_by_zero);
        return false;
    }

    return true;
}

Error *ErrorNewFormatVA(const char *id, const char *format, va_list args) {
    Error *err = nullptr;
    String *msg;

    msg = StringFormat(format, args);

    if (msg != nullptr)
        err = ErrorNew(id, msg);

    Release(msg);

    if (msg == nullptr || err == nullptr) {
        Release(err);

        err = (Error *) argon::vm::GetLastError();
        if (err == error_oom) {
            Release(err);
            argon::vm::Panic((ArObject *) error_err_oom);
            return nullptr;
        }

        // re-throw error!
        argon::vm::Panic((ArObject *) err);
        Release(err);

        argon::vm::Panic((ArObject *) error_while_error);
        return nullptr;
    }

    return err;
}

Error *argon::vm::datatype::ErrorNewFormat(const char *id, const char *format, ...) {
    va_list args;
    Error *err;

    va_start(args, format);
    err = ErrorNewFormatVA(id, format, args);
    va_end(args);

    return err;
}

Error *argon::vm::datatype::ErrorNewFormat(const char *id, const char *format, ArObject *args) {
    String *msg = StringFormat(format, args);

    if (msg == nullptr)
        return nullptr;

    auto *error = ErrorNew(id, msg);

    Release(msg);

    return error;
}

Error *argon::vm::datatype::ErrorNew(ArObject *id, String *reason) {
    auto *err = MakeObject<Error>(type_error_);

    if (err != nullptr) {
        memory::MemoryZero(&err->detail, sizeof(HashMap<ArObject *, ArObject>));

        err->reason = (ArObject *) IncRef(reason);
        err->id = IncRef(id);
    }

    return err;
}

Error *argon::vm::datatype::ErrorNew(const char *id, String *reason) {
    Atom *aid;
    Error *err;

    if ((aid = AtomNew(id)) == nullptr)
        return nullptr;

    err = ErrorNew((ArObject *) aid, reason);

    Release(aid);
    return err;
}

Error *argon::vm::datatype::ErrorNew(const char *id, const char *reason) {
    Atom *aid;
    Error *err;
    String *sreason;

    if ((aid = AtomNew(id)) == nullptr)
        return nullptr;

    if ((sreason = StringNew(reason)) == nullptr) {
        Release(aid);
        return nullptr;
    }

    err = ErrorNew((ArObject *) aid, sreason);

    Release(aid);
    Release(sreason);
    return err;
}

void argon::vm::datatype::ErrorFormat(const char *id, const char *format, ...) {
    va_list args;
    Error *err;

    va_start(args, format);
    err = ErrorNewFormatVA(id, format, args);
    va_end(args);

    if (err != nullptr)
        argon::vm::Panic((ArObject *) err);

    Release(err);
}

void argon::vm::datatype::ErrorFormat(const char *id, const char *format, ArObject *args) {
    auto *error = ErrorNewFormat(id, format, args);

    if (error != nullptr)
        argon::vm::Panic((ArObject *) error);

    Release(error);
}