// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/integer.h>
#include <object/datatype/function.h>
#include <object/arobject.h>

#include "io.h"
#include "iomodule.h"

using namespace argon::object;
using namespace argon::modules::io;

ArObject *io_mode_read = nullptr;
ArObject *io_mode_write = nullptr;
ArObject *io_mode_append = nullptr;

ArObject *io_buf_mode_none = nullptr;
ArObject *io_buf_mode_line = nullptr;
ArObject *io_buf_mode_block = nullptr;

ArObject *io_seek_mode_start = nullptr;
ArObject *io_seek_mode_cur = nullptr;
ArObject *io_seek_mode_end = nullptr;

ARGON_FUNC_NATIVE(io_create, create, "", 1, false) {
    return nullptr;
}

ARGON_FUNC_NATIVE(io_open, open, "", 1, false) {
    return nullptr;
}

ARGON_FUNC_NATIVE(io_openfile, openfile, "", 2, false) {
    return nullptr;
}

const PropertyBulk io_bulk[] = {
        MODULE_BULK_EXPORT_TYPE("file", type_file_),

        MODULE_BULK_EXPORT_FUNCTION(io_create),
        MODULE_BULK_EXPORT_FUNCTION(io_open),
        MODULE_BULK_EXPORT_FUNCTION(io_openfile),
        {nullptr, nullptr, false, PropertyInfo()} // Sentinel
};

bool IOInit(Module *module) {
#define INIT_CONST_STR(name, string)                                    \
     if (name==nullptr && ((name = StringIntern(string)) == nullptr))   \
        return false

#define INIT_OBJ(fn, key, c_value)                                                                          \
    if ((value = fn(c_value)) == nullptr)                                                                   \
        return false;                                                                                       \
    if (!ModuleAddProperty(module, key, value, PropertyInfo(PropertyType::PUBLIC | PropertyType::CONST))) { \
        Release(value);                                                                                     \
        return false; }                                                                                     \
    Release(value)

    ArObject *value;

    // FileMode
    INIT_CONST_STR(io_mode_read, "O_READ");
    INIT_CONST_STR(io_mode_write, "O_WRITE");
    INIT_CONST_STR(io_mode_append, "O_APPEND");
    INIT_OBJ(IntegerNew, io_mode_read, (IntegerUnderlayer) FileMode::READ);
    INIT_OBJ(IntegerNew, io_mode_write, (IntegerUnderlayer) FileMode::WRITE);
    INIT_OBJ(IntegerNew, io_mode_append, (IntegerUnderlayer) FileMode::APPEND);

    // FileBufferMode
    INIT_CONST_STR(io_buf_mode_none, "BUF_NONE");
    INIT_CONST_STR(io_buf_mode_line, "BUF_LINE");
    INIT_CONST_STR(io_buf_mode_block, "BUF_BLOCK");
    INIT_OBJ(IntegerNew, io_buf_mode_none, (IntegerUnderlayer) FileBufferMode::NONE);
    INIT_OBJ(IntegerNew, io_buf_mode_line, (IntegerUnderlayer) FileBufferMode::LINE);
    INIT_OBJ(IntegerNew, io_buf_mode_block, (IntegerUnderlayer) FileBufferMode::BLOCK);

    // FileWhence
    INIT_CONST_STR(io_seek_mode_start, "SEEK_START");
    INIT_CONST_STR(io_seek_mode_cur, "SEEK_CUR");
    INIT_CONST_STR(io_seek_mode_end, "SEEK_END");
    INIT_OBJ(IntegerNew, io_seek_mode_start, (IntegerUnderlayer) FileWhence::START);
    INIT_OBJ(IntegerNew, io_seek_mode_cur, (IntegerUnderlayer) FileWhence::CUR);
    INIT_OBJ(IntegerNew, io_seek_mode_end, (IntegerUnderlayer) FileWhence::END);

    return true;

#undef INIT_CONST_STR
#undef INIT_OBJ
}

void IOFinalize(Module *module) {
    Release(&io_mode_read);
    Release(&io_mode_write);
    Release(&io_mode_append);

    Release(&io_buf_mode_none);
    Release(&io_buf_mode_line);
    Release(&io_buf_mode_block);

    Release(&io_seek_mode_start);
    Release(&io_seek_mode_cur);
    Release(&io_seek_mode_end);
}

const ModuleInit module_io = {
        "io",
        "Module IO provides support to I/O primitives to read and write file",
        io_bulk,
        IOInit,
        IOFinalize
};

argon::object::Module *argon::modules::io::IONew() {
    return ModuleNew(&module_io);
}
