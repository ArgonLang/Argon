// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "atom.h"
#include "boolean.h"
#include "error.h"
#include "integer.h"
#include "stringbuilder.h"

using namespace argon::vm::datatype;

Error *argon::vm::datatype::error_div_by_zero = nullptr;
Error *argon::vm::datatype::error_oom = nullptr;
Error *argon::vm::datatype::error_err_oom = nullptr;
Error *argon::vm::datatype::error_while_error = nullptr;

// Prototypes

Error *ErrorNewFormatVA(const char *id, const char *format, va_list args);

String *ReprDetails(const Error *self, const String *header);

void SetErrorOOM();

// ***

ARGON_FUNCTION(error_error, Error,
               "Create a new error.\n"
               "\n"
               "- Parameters:\n"
               "  - id: Atom representing an ID.\n"
               "  - reason: String containing the reason for the error.\n"
               "  - &kwargs: Containing additional information about the error.\n"
               "- Returns: New Error.\n",
               "a: id, s: reason", 0, true) {
    return (ArObject *) ErrorNew((Atom *) args[0], (String *) args[1], (Dict *) kwargs);
}

const FunctionDef error_methods[] = {
        error_error,

        ARGON_METHOD_SENTINEL
};

const ObjectSlots error_objslot = {
        error_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *error_get_item(const Error *self, ArObject *key) {
    auto *entry = self->detail.Lookup(key);
    if (entry == nullptr) {
        ErrorFormat(kKeyError[0], kKeyError[1], key);

        return nullptr;
    }

    return IncRef(entry->value);
}

ArObject *error_item_in(const Error *self, ArObject *key) {
    return BoolToArBool(self->detail.Lookup(key) != nullptr);
}

ArSize error_length(const Error *self) {
    return self->detail.length;
}

const SubscriptSlots error_subscript = {
        (ArSize_UnaryOp) error_length,
        (BinaryOp) error_get_item,
        nullptr,
        nullptr,
        nullptr,
        (BinaryOp) error_item_in
};

ArObject *error_compare(Error *self, ArObject *other, CompareMode mode) {
    const auto *o = (Error *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    if (self->detail.length != o->detail.length)
        return BoolToArBool(false);

    for (auto *cursor = self->detail.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        const auto *other_entry = o->detail.Lookup(cursor->key);

        if (other_entry == nullptr)
            return BoolToArBool(false);

        if (!Equal(cursor->value, other_entry->value))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArObject *error_repr(const Error *self) {
    String *header;
    String *id;
    String *reason;

    if ((id = (String *) Str((ArObject *) self->id)) == nullptr)
        return nullptr;

    if ((reason = (String *) Repr(self->reason)) == nullptr) {
        Release(self->reason);
        return nullptr;
    }

    header = StringFormat("%s(%s,%s)", type_error_->name, ARGON_RAW_STRING(id), ARGON_RAW_STRING(reason));

    Release(id);
    Release(reason);

    if (header == nullptr)
        return nullptr;

    if (self->detail.length > 0) {
        id = ReprDetails(self, header);

        Release(header);

        return (ArObject *) id;
    }

    return (ArObject *) header;
}

bool error_dtor(Error *self) {
    Release(self->id);
    Release(self->reason);

    self->detail.Finalize([](HEntry<ArObject, ArObject *> *entry) {
        Release(entry->key);
        Release(entry->value);
    });

    return true;
}

TypeInfo ErrorType = {
        AROBJ_HEAD_INIT_TYPE,
        "Error",
        nullptr,
        nullptr,
        sizeof(Error),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) error_dtor,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) error_compare,
        (UnaryConstOp) error_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &error_objslot,
        &error_subscript,
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

const char *ErrorIdFromErrno(int err) {
    switch (err) {
        case EPERM:
            return "OperationError";
        case ENOENT:
            return "FileError";
        case EINTR:
            return "InterruptError";
        case EAGAIN:
            return "TryAgainError";
        case EACCES:
            return "PermissionDeniedError";
        case EEXIST:
            return "FileError";
        case EISDIR:
            return "IsDirectoryError";
        case EPIPE:
            return "BrokenPipeError";
        default:
            return kOSError[0];
    }
}

Error *argon::vm::datatype::ErrorNew(Atom *id, String *reason) {
    auto *err = MakeObject<Error>(type_error_);
    if (err == nullptr) {
        SetErrorOOM();
        return nullptr;
    }

    memory::MemoryZero(&err->detail, sizeof(HashMap<ArObject *, ArObject>));

    err->reason = (ArObject *) IncRef(reason);
    err->id = IncRef(id);

    return err;
}

Error *argon::vm::datatype::ErrorNew(Atom *id, String *reason, Dict *aux) {
    ArObject *iter;
    Tuple *tmp;

    auto *err = MakeObject<Error>(type_error_);
    if (err == nullptr)
        goto ERROR;

    memory::MemoryZero(&err->detail, sizeof(HashMap<ArObject *, ArObject>));

    err->reason = (ArObject *) IncRef(reason);
    err->id = IncRef(id);

    if (aux != nullptr) {
        if (!err->detail.Initialize()) {
            Release(err);
            goto ERROR;
        }

        if ((iter = IteratorGet((ArObject *) aux, false)) == nullptr) {
            Release(err);
            goto ERROR;
        }

        while ((tmp = (Tuple *) IteratorNext(iter)) != nullptr) {
            auto *entry = err->detail.AllocHEntry();
            if (entry == nullptr) {
                Release(tmp);
                Release(iter);
                Release(err);

                goto ERROR;
            }

            entry->key = TupleGet(tmp, 0);
            entry->value = TupleGet(tmp, 1);

            if (!err->detail.Insert(entry)) {
                Release(entry->key);
                Release(entry->value);

                err->detail.FreeHEntry(entry);

                Release(tmp);
                Release(iter);
                Release(err);

                goto ERROR;
            }

            Release(tmp);
        }

        Release(iter);
    }

    return err;

    ERROR:
    if (argon::vm::CheckLastPanic(kOOMError[0])) {
        SetErrorOOM();
        return nullptr;
    }

    argon::vm::Panic((ArObject *) error_while_error);

    return nullptr;
}

Error *argon::vm::datatype::ErrorNew(const char *id, String *reason) {
    Atom *aid;
    Error *err;

    if ((aid = AtomNew(id)) == nullptr) {
        SetErrorOOM();
        return nullptr;
    }

    err = ErrorNew(aid, reason);

    Release(aid);

    return err;
}

Error *argon::vm::datatype::ErrorNew(const char *id, const char *reason) {
    Atom *aid;
    Error *err;
    String *sreason;

    if ((aid = AtomNew(id)) == nullptr) {
        SetErrorOOM();
        return nullptr;
    }

    if ((sreason = StringNew(reason)) == nullptr) {
        Release(aid);

        SetErrorOOM();
        return nullptr;
    }

    err = ErrorNew(aid, sreason);

    Release(aid);
    Release(sreason);

    return err;
}

Error *argon::vm::datatype::ErrorNewFromErrno(int err) {
    ArObject *entry_name = nullptr;
    ArObject *num = nullptr;
    Error *error;

    HEntry<ArObject, ArObject *> *entry;

    if ((error = ErrorNew(ErrorIdFromErrno(err), strerror(err))) == nullptr)
        goto ERROR;

    if (!error->detail.Initialize()) {
        Release(error);
        return nullptr;
    }

    if ((entry_name = (ArObject *) StringNew("errno")) == nullptr)
        goto ERROR;

    if ((num = (ArObject *) IntNew(err)) == nullptr)
        goto ERROR;

    if ((entry = error->detail.AllocHEntry()) == nullptr)
        goto ERROR;

    entry->key = entry_name;
    entry->value = num;

    if (!error->detail.Insert(entry)) {
        error->detail.FreeHEntry(entry);
        goto ERROR;
    }

    return error;

    ERROR:
    Release(entry_name);
    Release(num);
    Release(error);

    if (argon::vm::CheckLastPanic(kOOMError[0])) {
        SetErrorOOM();
        return nullptr;
    }

    argon::vm::Panic((ArObject *) error_while_error);

    return nullptr;
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

    if (msg == nullptr) {
        if (argon::vm::CheckLastPanic(kOOMError[0])) {
            SetErrorOOM();
            return nullptr;
        }

        argon::vm::Panic((ArObject *) error_while_error);

        return nullptr;
    }

    auto *error = ErrorNew(id, msg);

    Release(msg);

    return error;
}

Error *ErrorNewFormatVA(const char *id, const char *format, va_list args) {
    Error *err;
    String *msg;

    if ((msg = StringFormat(format, args)) == nullptr) {
        if (argon::vm::CheckLastPanic(kOOMError[0])) {
            SetErrorOOM();
            return nullptr;
        }

        argon::vm::Panic((ArObject *) error_while_error);

        return nullptr;
    }

    err = ErrorNew(id, msg);

    Release(msg);

    return err;
}

String *ReprDetails(const Error *self, const String *header) {
    StringBuilder builder{};
    String *ret;

    builder.Write(header, 2 + 256);

    builder.Write((const unsigned char *) " {", 2, 0);

    for (auto *cursor = self->detail.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        auto *key = (String *) Repr(cursor->key);
        auto *value = (String *) Repr(cursor->value);

        if (key == nullptr || value == nullptr) {
            Release(key);
            Release(value);

            return nullptr;
        }

        if (!builder.Write(key, ARGON_RAW_STRING_LENGTH(value) + (cursor->iter_next == nullptr ? 3 : 4))) {
            Release(key);
            Release(value);

            return nullptr;
        }

        builder.Write((const unsigned char *) ": ", 2, 0);

        builder.Write(value, 0);

        if (cursor->iter_next != nullptr)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(key);
        Release(value);
    }

    builder.Write((const unsigned char *) "}", 1, 0);

    if ((ret = builder.BuildString()) == nullptr) {
        auto *err = builder.GetError();

        argon::vm::Panic((ArObject *) err);

        Release((ArObject **) &err);
    }

    return ret;
}

void argon::vm::datatype::ErrorFromErrno(int err) {
    auto *error = ErrorNewFromErrno(err);

    if (error != nullptr)
        argon::vm::Panic((ArObject *) error);

    Release(error);
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

void SetErrorOOM() {
    const auto *fiber = argon::vm::GetFiber();
    auto *panic = fiber->panic;

    Release(panic->object);

    panic->object = (ArObject *) IncRef(error_err_oom);
}