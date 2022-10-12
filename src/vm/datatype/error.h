// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ERROR_H_
#define ARGON_VM_DATATYPE_ERROR_H_

#include "arobject.h"
#include "arstring.h"
#include "hashmap.h"

namespace argon::vm::datatype {
    constexpr const char *kAssertionError[] = {
            (const char *) "AssertionError"
    };

    constexpr const char *kBufferError[] = {
            (const char *) "BufferError",
            (const char *) "buffer of object '%s' is not writable"
    };

    constexpr const char *kDivByZeroError[] = {
            (const char *) "DivByZero",
            (const char *) "division by zero"
    };

    constexpr const char *kErrorError[] = {
            (const char *) "ErrorError",
            (const char *) "an error occurred while creating an error"
    };

    constexpr const char *kNotImplementedError[] = {
            (const char *) "NotImplementedError",
            (const char *) "you must implement method %s"
    };

    constexpr const char *kOOMError[] = {
            (const char *) "OutOfMemory",
            (const char *) "out of memory",
            (const char *) "out of memory while creating an error"
    };

    constexpr const char *kOverflowError[] = {
            (const char *) "OverflowError"
    };

    constexpr const char *kRuntimeError[] = {
            (const char *) "RuntimeError",
            (const char *) "unsupported operand '%s' for type '%s'",
            (const char *) "unsupported operand '%s' for: '%s' and '%s'"
    };

    constexpr const char *kTypeError[] = {
            (const char *) "TypeError",
    };

    constexpr const char *kUnassignableError[] = {
            (const char *) "UnassignableError",
            (const char *) "unable to assign value to constant '%s'"
    };

    constexpr const char *kUndeclaredeError[] = {
            (const char *) "UndeclaredError",
            (const char *) "'%s' undeclared global variable"
    };

    constexpr const char *kUnhashableError[] = {
            (const char *) "Unhashable",
            (const char *) "unhashable type: '%s'"
    };

    constexpr const char *kUnicodeError[] = {
            (const char *) "UnicodeError",
            (const char *) "can't decode byte 0x%x in unicode sequence"
    };

    constexpr const char *kValueError[] = {
            (const char *) "ValueError"
    };

    struct Error {
        AROBJ_HEAD;

        ArObject *reason;
        ArObject *id;

        HashMap<ArObject *, ArObject> detail;
    };
    extern const TypeInfo *type_error_;

    // Singleton
    extern Error *error_div_by_zero;
    extern Error *error_oom;
    extern Error *error_err_oom;
    extern Error *error_while_error;

    bool ErrorInit();

    Error *ErrorNewFormat(const char *id, const char *format, ...);

    Error *ErrorFormat(const char *id, const char *format, ...);

    Error *ErrorNew(ArObject *id, String *reason);

    Error *ErrorNew(const char *id, String *reason);

    Error *ErrorNew(const char *id, const char *reason);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_ERROR_H_

