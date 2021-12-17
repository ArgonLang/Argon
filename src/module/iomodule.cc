// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/arobject.h>
#include <object/datatype/integer.h>
#include <object/datatype/io/io.h>
#include <object/datatype/function.h>

#include "modules.h"

using namespace argon::object;
using namespace io;

const PropertyBulk io_bulk[] = {
        MODULE_EXPORT_TYPE_ALIAS("file", type_file_),
        MODULE_EXPORT_SENTINEL
};

bool IOInit(Module *module) {
#define INIT_OBJ(fn, c_key, c_value)                                            \
    if((key = StringIntern(c_key)) == nullptr)                                  \
        return false;                                                           \
    if ((value = fn(c_value)) == nullptr)                                       \
        return false;                                                           \
    if (!ModuleAddProperty(module, key, value, MODULE_ATTRIBUTE_PUB_CONST)) {   \
        Release(value);                                                         \
        return false; }                                                         \
    Release(key);                                                               \
    Release(value)

    ArObject *key;
    ArObject *value;

    // FileMode
    INIT_OBJ(IntegerNew, "O_READ", (IntegerUnderlying) FileMode::READ);
    INIT_OBJ(IntegerNew, "O_WRITE", (IntegerUnderlying) FileMode::WRITE);
    INIT_OBJ(IntegerNew, "O_APPEND", (IntegerUnderlying) FileMode::APPEND);

    // FileBufferMode
    INIT_OBJ(IntegerNew, "BUF_NONE", (IntegerUnderlying) FileBufferMode::NONE);
    INIT_OBJ(IntegerNew, "BUF_LINE", (IntegerUnderlying) FileBufferMode::LINE);
    INIT_OBJ(IntegerNew, "BUF_BLOCK", (IntegerUnderlying) FileBufferMode::BLOCK);

    // FileWhence
    INIT_OBJ(IntegerNew, "SEEK_START", (IntegerUnderlying) FileWhence::START);
    INIT_OBJ(IntegerNew,  "SEEK_CUR", (IntegerUnderlying) FileWhence::CUR);
    INIT_OBJ(IntegerNew, "SEEK_END", (IntegerUnderlying) FileWhence::END);

    return true;
#undef INIT_OBJ
}

const ModuleInit module_io = {
        "io",
        "Module IO provides support to I/O primitives to read and write file",
        io_bulk,
        IOInit,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_io_ = &module_io;
