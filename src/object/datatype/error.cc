// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>

#include <vm/runtime.h>

#include "error.h"

using namespace argon::object;

#define ERROR_STATIC_INIT(base, name, ptr_name, type, obj)  \
base name {                                                 \
        {RefCount(RCType::STATIC), &type},                  \
        obj                                                 \
};                                                          \
ArObject *ptr_name = &name

Error *argon::object::ErrorNew(ArObject *obj) {
    auto error = ArObjectNew<Error>(RCType::INLINE, &error_error);

    if (error != nullptr)
        error->obj = obj;

    return error;
}

ArObject *argon::object::ErrorFormat(const TypeInfo *etype, const char *format, ...) {
    char *buf;
    ErrorStr *error;

    int sz;
    va_list args;

    va_start (args, format);
    sz = vsnprintf(nullptr, 0, format, args) + 1; // +1 is for '\0'
    va_end(args);

    if ((buf = (char *) argon::memory::Alloc(sz)) == nullptr) {
        argon::vm::Panic(OutOfMemoryError);
        return nullptr;
    }

    if ((error = ArObjectNew<ErrorStr>(RCType::INLINE, etype)) == nullptr) {
        argon::memory::Free(buf);
        argon::vm::Panic(OutOfMemoryError);
        return nullptr;
    }

    va_start(args, format);
    vsnprintf(buf, sz, format, args);
    va_end(args);

    error->msg = buf;

    argon::vm::Panic(error);
    Release(error);

    return nullptr;
}

const TypeInfo *argon::object::ErrorFromErrno() {
    switch (errno) {
        case EPERM:
            return &error_file_access;
        case ENOENT:
            return &error_file_exists;
        case EINTR:
            return &error_io_interrupted;
        case EAGAIN:
            return &error_io_blocking;
        case EACCES:
            return &error_file_access;
        case EEXIST:
            return &error_file_exists;
        case EISDIR:
            return &error_isa_directory;
        case EPIPE:
            return &error_io_broken_pipe;
        default:
            return &error_io;
    }
}

void argon::object::__error_str_cleanup(ArObject *obj) { argon::memory::Free((char *) ((ErrorStr *) obj)->msg); }

void argon::object::__error_error_cleanup(ArObject *obj) { Release(((Error *) obj)->obj); }

// ArithmeticError
ERROR_STATIC_INIT(ErrorStr, ZeroDivision, argon::object::ZeroDivisionError, error_zero_division_error,
                  "divide by zero");

// RuntimeError
ERROR_STATIC_INIT(ErrorStr, OutOfMemory, argon::object::OutOfMemoryError, error_oo_memory, "out of memory");

