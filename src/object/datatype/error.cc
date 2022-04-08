// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdarg>
#include <cerrno>

#include <vm/runtime.h>

#include <object/datatype/io/io.h>
#include "bool.h"
#include "integer.h"
#include "nil.h"
#include "tuple.h"
#include "error.h"

using namespace argon::object;

const ArObject *argon::object::error_types = nullptr;

ARGON_METHOD5(error_t_, unwrap, "", 0, false) {
    return ErrorFormat(type_not_implemented_, "you must implement %s::unwrap", AR_TYPE_NAME(self));
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
        nullptr,
        nullptr,
        nullptr,
        -1
};

const TypeInfo ErrorWrap = {
        TYPEINFO_STATIC_INIT,
        "ErrorWrap",
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
const TypeInfo *argon::object::type_error_wrap_ = &ErrorWrap;

ARGON_FUNCTION5(error_, new, "", 1, false) {
    return ErrorNew(((Function *) func)->base, *argv);
}

ARGON_METHOD5(error_, unwrap, "", 0, false) {
    auto *err = (Error *) self;
    return err->obj == nullptr ? IncRef(NilVal) : IncRef(err->obj);
}

const NativeFunc error_methods[] = {
        error_new_,
        error_unwrap_,
        ARGON_METHOD_SENTINEL
};

const NativeMember error_members[] = {
        ARGON_MEMBER("error", offsetof(Error, obj), NativeMemberType::AROBJECT, true),
        ARGON_MEMBER_SENTINEL
};

const TypeInfo *error_bases[] = {
        type_error_wrap_,
        nullptr
};

const ObjectSlots error_obj = {
        error_methods,
        error_members,
        error_bases,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *error_compare(Error *self, ArObject *other, CompareMode mode) {
    auto *o = (Error *) other;

    if (self == o)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(Equal(self->obj, o->obj));
}

ArObject *error_str(Error *self) {
    String *repr;
    String *ret;

    if (self->obj != nullptr) {
        repr = (String *) ToString(self->obj);

        ret = StringEmpty(repr) ? StringNew(AR_TYPE_NAME(self)) :
              StringNewFormat("%s: %s", AR_TYPE_NAME(self), repr->buffer);

        Release(repr);
        return ret;
    }

    return StringNew(AR_TYPE_NAME(self));
}

bool error_is_true(ArObject *self) {
    return true;
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
    TypeInfoFlags::STRUCT,                      \
    nullptr,                                    \
    (VoidUnaryOp)error_cleanup,                 \
    nullptr,                                    \
    (CompareOp)error_compare,                   \
    error_is_true,                              \
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

#define ERROR_SIMPLE_STATIC(name, ptr_name, etype)  \
const argon::object::Error name = {                 \
    {RefCount(RCType::STATIC), (etype)},            \
    nullptr                                         \
};                                                  \
Error *(ptr_name) = (Error*) &(name)

// Runtime error types
ERROR_SIMPLE(AccessViolation, "", argon::object::type_access_violation_);
ERROR_SIMPLE(AttributeError, "", argon::object::type_attribute_error_);
ERROR_SIMPLE(BufferError, "", argon::object::type_buffer_error_);
ERROR_SIMPLE(ExhaustedIteratorError, "", argon::object::type_exhausted_iterator_);
ERROR_SIMPLE(ExhaustedGeneratorError, "", argon::object::type_exhausted_generator_);
ERROR_SIMPLE(KeyNotFoundError, "", argon::object::type_key_not_found_);
ERROR_SIMPLE(ModuleNotFound, "", argon::object::type_module_not_found_);
ERROR_SIMPLE(NotImplemented, "", argon::object::type_not_implemented_);
ERROR_SIMPLE(OutOfMemoryError, "", type_out_of_memory_);
ERROR_SIMPLE(OverflowError, "", argon::object::type_overflow_error_);
ERROR_SIMPLE(RuntimeError, "", argon::object::type_runtime_error_);
ERROR_SIMPLE(RuntimeExit, "", argon::object::type_runtime_exit_error_);
ERROR_SIMPLE(ScopeError, "", argon::object::type_scope_error_);
ERROR_SIMPLE(TypeError, "", argon::object::type_type_error_);
ERROR_SIMPLE(UnassignableError, "", argon::object::type_unassignable_error_);
ERROR_SIMPLE(UndeclaredError, "", argon::object::type_undeclared_error_);
ERROR_SIMPLE(UnhashableError, "", argon::object::type_unhashable_error_);
ERROR_SIMPLE(UnicodeIndex, "", argon::object::type_unicode_index_error_);
ERROR_SIMPLE(ValueError, "", argon::object::type_value_error_);
ERROR_SIMPLE(RegexError, "", argon::object::type_regex_error_);
ERROR_SIMPLE(ZeroDivisionError, "", type_zero_division_);

// Compiler errors
ERROR_SIMPLE(SyntaxError, "", argon::object::type_syntax_error_);
ERROR_SIMPLE(CompileError, "", argon::object::type_compile_error_);

// IO Error
ERROR_SIMPLE(BlockingIO, "", argon::object::type_blocking_io_);
ERROR_SIMPLE(BrokenPipe, "", argon::object::type_broken_pipe_);
ERROR_SIMPLE(FileAccessError, "", argon::object::type_file_access_);
ERROR_SIMPLE(FileExistsError, "", argon::object::type_file_exists_);
ERROR_SIMPLE(FileNotFoundError, "", argon::object::type_file_not_found_);
ERROR_SIMPLE(IOError, "", argon::object::type_io_error_);
ERROR_SIMPLE(InterruptedError, "", argon::object::type_interrupted_error_);
ERROR_SIMPLE(IsDirectoryError, "", argon::object::type_is_directory_);
ERROR_SIMPLE(GAIError, "", argon::object::type_gai_error_);
ERROR_SIMPLE(WSAError, "", argon::object::type_wsa_error_);

ERROR_SIMPLE(OSError, "", argon::object::type_os_error_);

ERROR_SIMPLE_STATIC(OutOfMemory, argon::object::error_out_of_memory, type_out_of_memory_);
ERROR_SIMPLE_STATIC(ZeroDivision, argon::object::error_zero_division, type_zero_division_);
ERROR_SIMPLE_STATIC(ExhaustedGenerator, argon::object::error_exhausted_generator, type_exhausted_generator_);

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
            return type_os_error_;
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

ArObject *argon::object::ErrorFormat(const TypeInfo *etype, const char *format, ArObject *args) {
    ArObject *err;
    String *msg;

    if ((msg = StringCFormat(format, args)) == nullptr)
        return nullptr;

    err = ErrorNew(etype, msg);
    Release(msg);

    if (err == nullptr)
        return nullptr;

    argon::vm::Panic(err);
    Release(err);

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

    err = ErrorNew(etype, msg);
    Release(msg);

    if (err == nullptr)
        return nullptr;

    argon::vm::Panic(err);
    Release(err);

    return nullptr;
}

bool ErrorStaticInit(const char *instance_name, const char *message, Error *error) {
    ArObject *tmp;
    bool ok;

    if (message != nullptr) {
        if ((tmp = StringNew(message)) == nullptr)
            return false;

        if (error->obj != nullptr)
            Release(error->obj);

        error->obj = tmp;
    }

    ok = NamespaceNewSymbol((Namespace *) error_types, instance_name, (ArObject *) error,
                            PropertyType::PUBLIC | PropertyType::CONST);

    Release(error);
    return ok;
}

ArObject *argon::object::ErrorFormatNoPanic(const TypeInfo *etype, const char *format, ...) {
    va_list args;
    ArObject *err;
    String *msg;

    va_start(args, format);
    msg = StringNewFormat(format, args);
    va_end(args);

    if (msg == nullptr)
        return nullptr;

    err = ErrorNew(etype, msg);
    Release(msg);

    return err;
}

bool argon::object::ErrorInit() {
#define INIT(ERR_TYPE)                                                                          \
    if(!TypeInit((TypeInfo*) (ERR_TYPE), nullptr))                                              \
        return false;                                                                           \
    if(!NamespaceNewSymbol((Namespace*)error_types, (ERR_TYPE)->name, (ArObject*) (ERR_TYPE),   \
        PropertyType::CONST | PropertyType::PUBLIC))                                            \
        return false

    if ((error_types = (ArObject *) NamespaceNew()) == nullptr)
        return false;

    INIT(type_out_of_memory_);

    if (!ErrorStaticInit("OutOfMemory", "out of memory", error_out_of_memory))
        return false;

    INIT(argon::object::type_error_wrap_);
    INIT(argon::object::type_access_violation_);
    INIT(argon::object::type_attribute_error_);
    INIT(argon::object::type_buffer_error_);
    INIT(argon::object::type_exhausted_iterator_);
    INIT(argon::object::type_exhausted_generator_);
    INIT(argon::object::type_key_not_found_);
    INIT(argon::object::type_module_not_found_);
    INIT(argon::object::type_not_implemented_);
    INIT(argon::object::type_overflow_error_);
    INIT(argon::object::type_runtime_error_);
    INIT(argon::object::type_runtime_exit_error_);
    INIT(argon::object::type_scope_error_);
    INIT(argon::object::type_type_error_);
    INIT(argon::object::type_unassignable_error_);
    INIT(argon::object::type_undeclared_error_);
    INIT(argon::object::type_unhashable_error_);
    INIT(argon::object::type_unicode_index_error_);
    INIT(argon::object::type_value_error_);
    INIT(type_zero_division_);

    if (!ErrorStaticInit("ZeroDivision", "zero division error", error_zero_division))
        return false;

    if (!ErrorStaticInit("ExhaustedGenerator", nullptr, error_exhausted_generator))
        return false;

    // Compiler
    INIT(argon::object::type_syntax_error_);
    INIT(argon::object::type_compile_error_);

    // IO
    INIT(argon::object::type_blocking_io_);
    INIT(argon::object::type_broken_pipe_);
    INIT(argon::object::type_file_access_);
    INIT(argon::object::type_file_exists_);
    INIT(argon::object::type_file_not_found_);
    INIT(argon::object::type_io_error_);
    INIT(argon::object::type_interrupted_error_);
    INIT(argon::object::type_is_directory_);
    INIT(argon::object::type_gai_error_);
    INIT(argon::object::type_wsa_error_);

    INIT(argon::object::type_os_error_);

    return true;
}

void argon::object::ErrorPrint(ArObject *object) {
    char emsg[100];
    ArObject *last_error;
    String *str;
    io::File *err;

    int count = 0;

    if (object == nullptr)
        return;

    err = (io::File *) vm::ContextRuntimeGetProperty("stderr", nullptr);

    if (err == nullptr || !AR_TYPEOF(err, io::type_file_)) {
        // Silently discard error and continue
        Release(err);
        return;
    }

    last_error = IncRef(object);

    while ((str = (String *) ToString(last_error)) == nullptr && count++ < 3) {
        snprintf(emsg, 100, "an error occurred while trying to show a previous error from '%s' object:\n",
                 AR_TYPE_NAME(last_error));

        Release(last_error);

        last_error = vm::GetLastError();

        io::WriteString(err, emsg);

        for (int i = 0; i < count; i++)
            io::WriteString(err, "\t");
    }

    Release(last_error);

    if (io::WriteObject(err, str) >= 0)
        io::WriteString(err, "\n");

    Release(str);
    Release(err);
}