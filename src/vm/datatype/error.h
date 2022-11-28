// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ERROR_H_
#define ARGON_VM_DATATYPE_ERROR_H_

#include "arobject.h"
#include "arstring.h"
#include "hashmap.h"

namespace argon::vm::datatype {
    constexpr const char *kAccessViolationError[] = {
            (const char *) "AccessViolationError",
            (const char *) "access violation, member '%s' of '%s' are private",
            (const char *) "in order to access to non const member '%s' an instance of '%s' is required"
    };

    constexpr const char *kAssertionError[] = {
            (const char *) "AssertionError"
    };

    constexpr const char *kAttributeError[] = {
            (const char *) "AttributeError",
            (const char *) "object of type '%s' does not support dot(.) operator",
            (const char *) "object of type '%s' does not support scope(::) operator",
            (const char *) "unknown attribute '%s' of instance '%s'"
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
            (const char *) "you must implement method %s",
            (const char *) "operator '%s' not supported between instance of '%s' and '%s'"
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
            (const char *) "unsupported operand '%s' for: '%s' and '%s'",
            (const char *) "malformed code object, code::statics out of bound %d/%d",
            (const char *) "unknown native type for the %s::%s property"
    };

    constexpr const char *kTypeError[] = {
            (const char *) "TypeError",
            (const char *) "a type is required, not an instance of %s",
            (const char *) "expected '%s' got '%s'",
            (const char *)"%s() takes %d argument, but %d were given",
            (const char *)"%s() does not accept keyword arguments",
            (const char *)"method %s doesn't apply to '%s' type",
            (const char *)"defer does not support %s (async function)",
            (const char *)"defer does not support %s (generator function)"
    };

    constexpr const char *kUnassignableError[] = {
            (const char *) "UnassignableError",
            (const char *) "unable to assign value to constant '%s'",
            (const char *) "%s::%s is read-only"
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
            (const char *) "can't decode byte 0x%x in unicode sequence",
            (const char *) "unable to index a unicode string",
            (const char *) "unable to slice a unicode string"
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

    Error *ErrorNew(ArObject *id, String *reason);

    Error *ErrorNew(const char *id, String *reason);

    Error *ErrorNew(const char *id, const char *reason);

    void ErrorFormat(const char *id, const char *format, ...);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_ERROR_H_

