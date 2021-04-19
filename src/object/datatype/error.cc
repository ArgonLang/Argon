// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>
#include <cerrno>

#include <vm/runtime.h>

#include "integer.h"
#include "nil.h"
#include "tuple.h"
#include "error.h"

using namespace argon::object;

const ArObject *argon::object::error_types = nullptr;

ArObject *argon::object::error_out_of_memory = nullptr;
ArObject *argon::object::error_zero_division = nullptr;

ARGON_METHOD5(error_t_, unwrap, "", 0, false) {
    return IncRef(NilVal);
}

const NativeFunc error_t_methods[] = {
        error_t_unwrap_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots error_t_obj = {
        error_t_methods,
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

ARGON_METHOD5(error_, unwrap, "", 0, false) {
    return IncRef(((Error *) self)->obj);
}

const NativeFunc error_methods[] = {
        error_unwrap_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots error_obj = {
        error_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *error_str(Error *self) {
    auto *tmp = (String *) ToString(self->obj);
    String *ret;

    ret = StringNewFormat("%s: %s", AR_TYPE_NAME(self), tmp->buffer);
    Release(tmp);

    return ret;
}

void error_cleanup(Error *self) {
    Release(self->obj);
}

#define ERROR_SIMPLE(name, doc, ptr_name)       \
const TypeInfo name = {                         \
    TYPEINFO_STATIC_INIT,                       \
    #name,                                      \
    #doc,                                       \
    sizeof(Error),                              \
    TypeInfoFlags::BASE,                        \
    nullptr,                                    \
    (VoidUnaryOp)error_cleanup,                 \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    (UnaryOp)error_str,                         \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    &error_obj,                                 \
    nullptr,                                    \
    nullptr,                                    \
    nullptr,                                    \
    nullptr                                     \
};                                              \
const TypeInfo *(ptr_name) = &(name)

// Runtime error types
ERROR_SIMPLE(AccessViolation, "", argon::object::type_access_violation_);
ERROR_SIMPLE(AttributeError, "", argon::object::type_attribute_error_);
ERROR_SIMPLE(BufferError, "", argon::object::type_buffer_error_);
ERROR_SIMPLE(ExhaustedIteratorError, "", argon::object::type_exhausted_iterator_);
ERROR_SIMPLE(ModuleNotFound, "", argon::object::type_module_not_found_);
ERROR_SIMPLE(NotImplemented, "", argon::object::type_not_implemented_);
ERROR_SIMPLE(OutOfMemory, "", type_out_of_memory_);
ERROR_SIMPLE(OverflowError, "", argon::object::type_overflow_error_);
ERROR_SIMPLE(RuntimeError, "", argon::object::type_runtime_error_);
ERROR_SIMPLE(ScopeError, "", argon::object::type_scope_error_);
ERROR_SIMPLE(TypeError, "", argon::object::type_type_error_);
ERROR_SIMPLE(UnassignableError, "", argon::object::type_unassignable_error_);
ERROR_SIMPLE(UndeclaredError, "", argon::object::type_undeclared_error_);
ERROR_SIMPLE(UnhashableError, "", argon::object::type_unhashable_error_);
ERROR_SIMPLE(UnicodeIndex, "", argon::object::type_unicode_index_error_);
ERROR_SIMPLE(ValueError, "", argon::object::type_value_error_);
ERROR_SIMPLE(ZeroDivision, "", type_zero_division_);

// IO Error
ERROR_SIMPLE(BlockingIO, "", argon::object::type_blocking_io_);
ERROR_SIMPLE(BrokenPipe, "", argon::object::type_broken_pipe_);
ERROR_SIMPLE(FileAccessError, "", argon::object::type_file_access_);
ERROR_SIMPLE(FileExistsError, "", argon::object::type_file_exists_);
ERROR_SIMPLE(FileNotFoundError, "", argon::object::type_file_not_found_);
ERROR_SIMPLE(IOError, "", argon::object::type_io_error_);
ERROR_SIMPLE(InterruptedError, "", argon::object::type_interrupted_error_);
ERROR_SIMPLE(IsDirectoryError, "", argon::object::type_is_directory_);

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

ArObject *argon::object::ErrorSetFromErrno() {
    ArObject *err = ErrorNewFromErrno();

    if (err != nullptr) {
        argon::vm::Panic(err);
        Release(err);
    }

    return nullptr;
}

ArObject *argon::object::ErrorFormat(const TypeInfo *etype, const char *format, ...) {
    va_list args;
    ArObject *err;
    String *msg;

    va_start(args, format);
    msg = StringNewFormat(format, args);
    va_end(args);

    if (msg == nullptr)
        return nullptr;

    if ((err = ErrorNew(etype, msg)) == nullptr) {
        Release(msg);
        return nullptr;
    }

    argon::vm::Panic(err);
    Release(err);

    return nullptr;
}

bool argon::object::ErrorInit() {
#define INIT(ERR_TYPE)                                                          \
    if(!TypeInit((TypeInfo*) (ERR_TYPE), nullptr))                              \
        return false;                                                           \
    if(!NamespaceNewSymbol((Namespace*)error_types, (ERR_TYPE)->name, (ArObject*) (ERR_TYPE),    \
        PropertyInfo(PropertyType::CONST | PropertyType::PUBLIC)))              \
        return false

    String *msg;

    if ((error_types = (ArObject *) NamespaceNew()) == nullptr)
        return false;

    INIT(type_out_of_memory_);

    if ((msg = StringNew("out of memory")) == nullptr)
        return false;

    if ((error_out_of_memory = ErrorNew(type_out_of_memory_, msg)) == nullptr) {
        Release(msg);
        return false;
    }

    INIT(argon::object::type_access_violation_);
    INIT(argon::object::type_attribute_error_);
    INIT(argon::object::type_buffer_error_);
    INIT(argon::object::type_exhausted_iterator_);
    INIT(argon::object::type_module_not_found_);
    INIT(argon::object::type_not_implemented_);
    INIT(argon::object::type_overflow_error_);
    INIT(argon::object::type_runtime_error_);
    INIT(argon::object::type_scope_error_);
    INIT(argon::object::type_type_error_);
    INIT(argon::object::type_unassignable_error_);
    INIT(argon::object::type_undeclared_error_);
    INIT(argon::object::type_unhashable_error_);
    INIT(argon::object::type_unicode_index_error_);
    INIT(argon::object::type_value_error_);
    INIT(type_zero_division_);

    if ((msg = StringNew("zero division error")) == nullptr)
        return false;

    if ((error_zero_division = ErrorNew(type_zero_division_, msg)) == nullptr) {
        Release(msg);
        return false;
    }

    // IO
    INIT(argon::object::type_blocking_io_);
    INIT(argon::object::type_broken_pipe_);
    INIT(argon::object::type_file_access_);
    INIT(argon::object::type_file_exists_);
    INIT(argon::object::type_file_not_found_);
    INIT(argon::object::type_io_error_);
    INIT(argon::object::type_interrupted_error_);
    INIT(argon::object::type_is_directory_);

    return true;
}
