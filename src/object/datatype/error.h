// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ERROR_H_
#define ARGON_OBJECT_ERROR_H_

#include <utils/macros.h>

#include <object/arobject.h>

namespace argon::object {
    struct Error : ArObject {
        ArObject *obj;
    };

    extern const ObjectSlots *_error_objs;

#define ARGON_ERROR_TYPE_SIMPLE(name, doc, ptr_name)    \
const TypeInfo name = {                                 \
    TYPEINFO_STATIC_INIT,                               \
    #name,                                              \
    #doc,                                               \
    sizeof(Error),                                      \
    TypeInfoFlags::STRUCT,                              \
    nullptr,                                            \
    (VoidUnaryOp)ErrorCleanup,                          \
    nullptr,                                            \
    (CompareOp)ErrorCompare,                            \
    TypeInfo_IsTrue_True,                               \
    nullptr,                                            \
    (UnaryOp)ErrorStr,                                  \
    nullptr,                                            \
    nullptr,                                            \
    nullptr,                                            \
    nullptr,                                            \
    nullptr,                                            \
    nullptr,                                            \
    _error_objs,                                        \
    nullptr,                                            \
    nullptr,                                            \
    nullptr,                                            \
    nullptr                                             \
};                                                      \
const TypeInfo *(ptr_name) = &(name)

#define ARGON_ERROR_SIMPLE_STATIC(name, ptr_name, etype)    \
const argon::object::Error name = {                         \
    {RefCount(RCType::STATIC), (etype)},                    \
    nullptr                                                 \
};                                                          \
Error *(ptr_name) = (Error*) &(name)

    ArObject *ErrorCompare(Error *self, ArObject *other, CompareMode mode);

    ArObject *ErrorFormat(const TypeInfo *etype, const char *format, ArObject *args);

    ArObject *ErrorFormat(const TypeInfo *etype, const char *format, ...);

    ArObject *ErrorFormatNoPanic(const TypeInfo *etype, const char *format, ...);

    ArObject *ErrorNew(const TypeInfo *type, ArObject *object);

    ArObject *ErrorNew(const TypeInfo *type, const char *emsg);

    ArObject *ErrorNewFromErrno();

    ArObject *ErrorSetFromErrno();

#ifdef _ARGON_PLATFORM_WINDOWS

    ArObject *ErrorNewFromWinError();

    ArObject *ErrorSetFromWinError();

    ArSize ErrorGetLast();

#endif

    ArObject *ErrorStr(Error *self);

    ArObject *ErrorTupleFromErrno();

    bool ErrorInit();

    const TypeInfo *ErrorTypeFromErrno();

    void ErrorCleanup(Error *self);

    void ErrorPrint(ArObject *object);

    extern const ArObject *error_types;

    extern const TypeInfo *type_error_wrap_;

    // Runtime errors
    extern const TypeInfo *type_access_violation_;
    extern const TypeInfo *type_attribute_error_;
    extern const TypeInfo *type_buffer_error_;
    extern const TypeInfo *type_exhausted_iterator_;
    extern const TypeInfo *type_exhausted_generator_;
    extern const TypeInfo *type_key_not_found_;
    extern const TypeInfo *type_module_not_found_;
    extern const TypeInfo *type_not_implemented_;
    extern const TypeInfo *type_overflow_error_;
    extern const TypeInfo *type_override_error_;
    extern const TypeInfo *type_runtime_error_;
    extern const TypeInfo *type_runtime_exit_error_;
    extern const TypeInfo *type_scope_error_;
    extern const TypeInfo *type_type_error_;
    extern const TypeInfo *type_unassignable_error_;
    extern const TypeInfo *type_undeclared_error_;
    extern const TypeInfo *type_unhashable_error_;
    extern const TypeInfo *type_unimplemented_error_;
    extern const TypeInfo *type_unicode_index_error_;
    extern const TypeInfo *type_value_error_;
    extern const TypeInfo *type_regex_error_;

    // Compiler errors
    extern const TypeInfo *type_syntax_error_;
    extern const TypeInfo *type_compile_error_;

    // IO errors
    extern const TypeInfo *type_blocking_io_;
    extern const TypeInfo *type_broken_pipe_;
    extern const TypeInfo *type_file_access_;
    extern const TypeInfo *type_file_exists_;
    extern const TypeInfo *type_file_not_found_;
    extern const TypeInfo *type_io_error_;
    extern const TypeInfo *type_interrupted_error_;
    extern const TypeInfo *type_is_directory_;
    extern const TypeInfo *type_gai_error_;
    extern const TypeInfo *type_wsa_error_;

    extern const TypeInfo *type_os_error_;


    // ExportedErrors
    extern Error *error_zero_division;
    extern Error *error_out_of_memory;
    extern Error *error_exhausted_generator;

} // namespace argon::object

#endif // !ARGON_OBJECT_ERROR_H_
