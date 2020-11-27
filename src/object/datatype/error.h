// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ERROR_H_
#define ARGON_OBJECT_ERROR_H_

#include <object/arobject.h>

namespace argon::object {
    struct Error : ArObject {
        ArObject *obj;
    };

    struct ErrorStr : ArObject {
        const char *msg;
    };

    void __error_str_cleanup(ArObject *obj);

    void __error_error_cleanup(ArObject *obj);

    Error *ErrorNew(ArObject *obj);

    ArObject *ErrorFormat(const TypeInfo *etype, const char *format, ...);

    const TypeInfo *ErrorFromErrno();

#define ERROR_NEW_TYPE(type_name, name, base, cleanup, obj_actions) \
const TypeInfo error_##type_name = {                                \
        TYPEINFO_STATIC_INIT,                                       \
        (const unsigned char *) #name,                              \
        sizeof(base),                                               \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        obj_actions,                                                \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        cleanup                                                     \
}

#define ERROR_STR_NEW_TYPE(type_name, name, obj_actions)                        \
ERROR_NEW_TYPE(type_name, name, ErrorStr, __error_str_cleanup, obj_actions)

    // Generic error that can encapsulate any ArObject
    ERROR_NEW_TYPE(error, error, Error, __error_error_cleanup, nullptr);

    // ArithmeticErrors
    ERROR_STR_NEW_TYPE(zero_division_error, ZeroDivisionError, nullptr);

    // Runtime errors
    ERROR_STR_NEW_TYPE(oo_memory, OutOfMemory, nullptr);
    ERROR_STR_NEW_TYPE(type_error, TypeError, nullptr);
    ERROR_STR_NEW_TYPE(not_implemented, NotImplemented, nullptr);
    ERROR_STR_NEW_TYPE(undeclared_variable, UndeclaredVariable, nullptr);
    ERROR_STR_NEW_TYPE(unassignable_variable, UnassignableVariable, nullptr);
    ERROR_STR_NEW_TYPE(attribute_error, AttributeError, nullptr);
    ERROR_STR_NEW_TYPE(scope_error, ScopeError, nullptr);
    ERROR_STR_NEW_TYPE(access_violation, AccessViolation, nullptr);
    ERROR_STR_NEW_TYPE(runtime_error, RuntimeError, nullptr);
    ERROR_STR_NEW_TYPE(module_notfound, ModuleNotFound, nullptr);
    ERROR_STR_NEW_TYPE(overflow_error, OverflowError, nullptr);

    // IO
    ERROR_STR_NEW_TYPE(io, IOError, nullptr);
    ERROR_STR_NEW_TYPE(io_blocking, BlockingIOError, nullptr);
    ERROR_STR_NEW_TYPE(io_broken_pipe, BrokenPipeError, nullptr);
    ERROR_STR_NEW_TYPE(io_interrupted, InterruptedError, nullptr);

    ERROR_STR_NEW_TYPE(isa_directory, IsADirectoryError, nullptr);
    ERROR_STR_NEW_TYPE(file_exists, FileExistsError, nullptr);
    ERROR_STR_NEW_TYPE(file_notfound, FileNotFound, nullptr);
    ERROR_STR_NEW_TYPE(file_access, FileAccessError, nullptr);

#undef ERROR_STR_NEW_TYPE
#undef ERROR_NEW_TYPE

    // ExportedErrors
    extern ArObject *ZeroDivisionError;
    extern ArObject *OutOfMemoryError;

} // namespace argon::object

#endif // !ARGON_OBJECT_ERROR_H_
