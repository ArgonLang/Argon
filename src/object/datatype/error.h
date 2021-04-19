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

    ArObject *ErrorNew(const TypeInfo *type, ArObject *object);

    ArObject *ErrorNewFromErrno();

    ArObject *ErrorSetFromErrno();

    ArObject *ErrorTupleFromErrno();

    ArObject *ErrorFormat(const TypeInfo *etype, const char *format, ...);

    const TypeInfo *ErrorTypeFromErrno();

    bool ErrorInit();

    extern const ArObject *error_types;

    // Runtime errors
    extern const TypeInfo *type_access_violation_;
    extern const TypeInfo *type_attribute_error_;
    extern const TypeInfo *type_buffer_error_;
    extern const TypeInfo *type_exhausted_iterator_;
    extern const TypeInfo *type_module_not_found_;
    extern const TypeInfo *type_not_implemented_;
    extern const TypeInfo *type_overflow_error_;
    extern const TypeInfo *type_runtime_error_;
    extern const TypeInfo *type_scope_error_;
    extern const TypeInfo *type_type_error_;
    extern const TypeInfo *type_unassignable_error_;
    extern const TypeInfo *type_undeclared_error_;
    extern const TypeInfo *type_unhashable_error_;
    extern const TypeInfo *type_unicode_index_error_;
    extern const TypeInfo *type_value_error_;

    // IO errors
    extern const TypeInfo *type_blocking_io_;
    extern const TypeInfo *type_broken_pipe_;
    extern const TypeInfo *type_file_access_;
    extern const TypeInfo *type_file_exists_;
    extern const TypeInfo *type_file_not_found_;
    extern const TypeInfo *type_io_error_;
    extern const TypeInfo *type_interrupted_error_;
    extern const TypeInfo *type_is_directory_;

    // ExportedErrors
    extern ArObject *error_zero_division;
    extern ArObject *error_out_of_memory;

} // namespace argon::object

#endif // !ARGON_OBJECT_ERROR_H_
