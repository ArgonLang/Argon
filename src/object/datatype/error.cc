// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>
#include <cerrno>

#include <vm/runtime.h>

#include "integer.h"
#include "tuple.h"
#include "error.h"
#include "nil.h"

using namespace argon::object;

Namespace *errors = nullptr;

ARGON_METHOD5(error_t_, error, "", 0, false) {
    return IncRef(NilVal);
}

const NativeFunc error_t_methods[] = {
        error_t_error_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots error_t_obj = {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

const TypeInfo type_error_t_ = {
        TYPEINFO_STATIC_INIT,
        "Error",
        nullptr,
        0,
        TypeInfoFlags::TRAIT,
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
        &error_t_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

#define ERROR_SIMPLE(name, doc, ptr_name)       \
const TypeInfo name = {                         \
    TYPEINFO_STATIC_INIT,                       \
    #name,                                      \
    #doc,                                       \
    0,                                          \
    TypeInfoFlags::BASE,                        \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr                                     \
};                                              \
const TypeInfo *(ptr_name) = &(name)

#define ERROR_SIMPLE_STATIC(name, doc, tp_name, err_name, error_msg)    \
    ERROR_SIMPLE(name, doc, tp_name);                                   \
    ErrorStr Static##name {                                             \
        {RefCount(RCType::STATIC), (tp_name)},                          \
        error_msg                                                       \
    };                                                                  \
    ArObject *argon::object::error_##err_name = &(Static##name)         \


// Runtime error types
ERROR_SIMPLE(AccessViolation, "", type_access_violation_);
ERROR_SIMPLE(AttributeError, "", type_attribute_error_);
ERROR_SIMPLE(BufferError, "", type_buffer_error_);
ERROR_SIMPLE(ExhaustedIteratorError, "", type_exhausted_iterator_);
ERROR_SIMPLE(ModuleNotFound, "", type_module_not_found_);
ERROR_SIMPLE(NotImplemented, "", type_not_implemented_);
ERROR_SIMPLE(OverflowError, "", type_overflow_error_);
ERROR_SIMPLE(RuntimeError, "", type_runtime_error_);
ERROR_SIMPLE(ScopeError, "", type_scope_error);
ERROR_SIMPLE(TypeError, "", type_type_error_);
ERROR_SIMPLE(UnassignableError, "", type_unassignable_error_);
ERROR_SIMPLE(UndeclaredError, "", type_undeclared_error_);
ERROR_SIMPLE(UnhashableError, "", type_unhashable_error_);
ERROR_SIMPLE(UnicodeIndex, "", type_unicode_index_error_);
ERROR_SIMPLE(ValueError, "", type_value_error_);

// IO Error
ERROR_SIMPLE(BlockingIO, "", type_blocking_io_);
ERROR_SIMPLE(BrokenPipe, "", type_broken_pipe_);
ERROR_SIMPLE(FileAccessError, "", type_file_access_);
ERROR_SIMPLE(FileExistsError, "", type_file_exists_);
ERROR_SIMPLE(FileNotFoundError, "", type_file_not_found_);
ERROR_SIMPLE(IOError, "", type_io_error_);
ERROR_SIMPLE(InterruptedError, "", type_interrupted_error_);
ERROR_SIMPLE(IsDirectoryError, "", type_is_directory_);

ERROR_SIMPLE_STATIC(OutOfMemory, "", type_out_of_memory_, out_of_memory, "out of memory");
ERROR_SIMPLE_STATIC(ZeroDivision, "", type_zero_division_, zero_division, "");

#undef ERROR_SIMPLE_STATIC
#undef ERROR_SIMPLE

ArObject *argon::object::ErrorTupleFromErrno() {
    Tuple *tuple = TupleNew(2);
    Integer *ernum;
    String *errMsg;

    if (tuple != nullptr) {
        if ((ernum = IntegerNew(errno)) == nullptr) {
            Release(tuple);
            return nullptr;
        }

        if ((errMsg = StringIntern(strerror(errno))) == nullptr) {
            Release(tuple);
            Release(ernum);
            return nullptr;
        }

        TupleInsertAt(tuple, 0, ernum);
        TupleInsertAt(tuple, 1, errMsg);

        Release(ernum);
        Release(errMsg);
    }

    return tuple;
}

const TypeInfo *argon::object::ErrorTypeFromErrno() {
    switch (errno) {
        case EPERM:
            return type_file_access_;
        case ENOENT:
            return type_file_exists_;
        case EINTR:
            return type_interrupted_error_;
        case EAGAIN:
            return type_blocking_io_;
        case EACCES:
            return type_file_access_;
        case EEXIST:
            return type_file_exists_;
        case EISDIR:
            return type_is_directory_;
        case EPIPE:
            return type_broken_pipe_;
        default:
            return type_io_error_;
    }
}

ArObject *argon::object::ErrorNew(const TypeInfo *type, ArObject *object) {
    auto *err = ArObjectNew<Error>(RCType::INLINE, type);

    if (err == nullptr)
        return argon::vm::Panic(error_out_of_memory);

    err->obj = IncRef(object);

    return err;
}

ArObject *argon::object::ErrorNewFromErrno() {
    ArObject *etuple = ErrorTupleFromErrno();
    const TypeInfo *etype = ErrorTypeFromErrno();
    ArObject *err;

    if (etuple == nullptr)
        return nullptr;

    err = ErrorNew(etype, etuple);
    Release(etuple);

    return err;
}

bool argon::object::ErrorInit() {
#define INIT(ERR_TYPE)                                                          \
    if(!TypeInit((TypeInfo*) (ERR_TYPE), nullptr))                              \
        return false;                                                           \
    if(!NamespaceNewSymbol(errors, (ERR_TYPE)->name, (ArObject*) (ERR_TYPE),    \
        PropertyInfo(PropertyType::CONST | PropertyType::PUBLIC)))              \
        return false

    if ((errors = NamespaceNew()) == nullptr)
        return false;

    INIT(type_out_of_memory_);
    INIT(type_zero_division_);

    INIT(type_access_violation_);
    INIT(type_attribute_error_);
    INIT(type_buffer_error_);
    INIT(type_exhausted_iterator_);
    INIT(type_module_not_found_);
    INIT(type_not_implemented_);
    INIT(type_overflow_error_);
    INIT(type_runtime_error_);
    INIT(type_scope_error);
    INIT(type_type_error_);
    INIT(type_unassignable_error_);
    INIT(type_undeclared_error_);
    INIT(type_unhashable_error_);
    INIT(type_unicode_index_error_);
    INIT(type_value_error_);

    INIT(type_blocking_io_);
    INIT(type_broken_pipe_);
    INIT(type_file_access_);
    INIT(type_file_exists_);
    INIT(type_file_not_found_);
    INIT(type_io_error_);
    INIT(type_interrupted_error_);
    INIT(type_is_directory_);

    return true;
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
        argon::vm::Panic(error_out_of_memory);
        return nullptr;
    }

    if ((error = ArObjectNew<ErrorStr>(RCType::INLINE, etype)) == nullptr) {
        argon::memory::Free(buf);
        argon::vm::Panic(error_out_of_memory);
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

ArObject *argon::object::__error_str(ErrorStr *self) {
    return StringNew(self->msg);
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
