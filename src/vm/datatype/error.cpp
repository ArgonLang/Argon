// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "atom.h"
#include "error.h"
#include "vm/runtime.h"

using namespace argon::vm::datatype;

Error *argon::vm::datatype::error_div_by_zero = nullptr;
Error *argon::vm::datatype::error_oom = nullptr;
Error *argon::vm::datatype::error_err_oom = nullptr;
Error *argon::vm::datatype::error_while_error = nullptr;

const TypeInfo ErrorType = {
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

Error *argon::vm::datatype::ErrorNewFormat(const char *id, const char *format, ...) {
    va_list args;
    Error *err;
    String *msg;

    va_start(args, format);
    msg = StringFormat(format, args);
    va_end(args);

    if (msg == nullptr) {
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

    if ((err = ErrorNew(id, msg)) == nullptr) {
        Release(msg);
        return nullptr;
    }

    Release(msg);
    return err;
}

Error *argon::vm::datatype::ErrorFormat(const char *id, const char *format, ...) {
    assert(false);
}

Error *argon::vm::datatype::ErrorNew(ArObject *id, String *reason) {
    auto *err = MakeObject<Error>(type_error_);

    if (err != nullptr) {
        if (err->detail.Initialize()) {
            Release(err);
            return nullptr;
        }

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
