// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/arobject.h>
#include <object/datatype/integer.h>
#include <object/datatype/io/io.h>

#include "modules.h"

using namespace argon::object;
using namespace io;

ARGON_FUNCTION5(io_, open, "Open file and return corresponding file object."
                           ""
                           "The operations that are allowed on the file and how these are performed are defined "
                           "by the mode parameter. The parameter mode value can be one or a combination of these:"
                           "  * O_READ"
                           "  * O_WRITE"
                           "  * O_APPEND"
                           ""
                           "- Parameters:"
                           "  - path: file path."
                           "  - mode: open mode."
                           "- Returns: (file, err)", 2, false) {
    File *file;
    char *path;

    FileMode flags;

    if (!CheckArgs("s:path,i:mode", func, argv, count))
        return nullptr;

    path = (char *) ((String *) argv[0])->buffer;
    flags = (FileMode) ((Integer *) argv[1])->integer;

    if ((file = Open(path, flags)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return ARGON_OBJECT_TUPLE_SUCCESS(file);
}

ARGON_FUNCTION5(io_, openfd, "Create file object from file descriptor."
                             ""
                             "The operations that are allowed on the file and how these are performed are defined "
                             "by the mode parameter. The parameter mode value can be one or a combination of these:"
                             "  * O_READ"
                             "  * O_WRITE"
                             "  * O_APPEND"
                             ""
                             "- Parameters:"
                             "  - fd: file descriptor (integer)."
                             "  - mode: open mode."
                             "- Returns: (file, err)", 2, false) {
    File *file;

    int fd;
    FileMode flags;

    if (!CheckArgs("i:fd,i:mode", func, argv, count))
        return nullptr;

    fd = (int) ((Integer *) argv[0])->integer;
    flags = (FileMode) ((Integer *) argv[1])->integer;

    if ((file = FdOpen(fd, flags)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return ARGON_OBJECT_TUPLE_SUCCESS(file);
}

const PropertyBulk io_bulk[] = {
        MODULE_EXPORT_TYPE(type_buffered_reader_),
        MODULE_EXPORT_TYPE(type_buffered_writer_),
        MODULE_EXPORT_TYPE(type_file_),
        MODULE_EXPORT_TYPE(type_readT_),
        MODULE_EXPORT_TYPE(type_writeT_),
        MODULE_EXPORT_FUNCTION(io_open_),
        MODULE_EXPORT_FUNCTION(io_openfd_),
        MODULE_EXPORT_SENTINEL
};

bool IOInit(Module *self) {
#define INIT_OBJ(c_key, c_value)                    \
    if(!ModuleAddIntConstant(self, c_key, c_value)) \
        return false

    // FileMode
    INIT_OBJ("O_READ", (int) FileMode::READ);
    INIT_OBJ("O_WRITE", (int) FileMode::WRITE);
    INIT_OBJ("O_APPEND", (int) FileMode::APPEND);

    // FileBufferMode
    INIT_OBJ("BUF_NONE", (int) FileBufferMode::NONE);
    INIT_OBJ("BUF_LINE", (int) FileBufferMode::LINE);
    INIT_OBJ("BUF_BLOCK", (int) FileBufferMode::BLOCK);

    // FileWhence
    INIT_OBJ("SEEK_START", (int) FileWhence::START);
    INIT_OBJ("SEEK_CUR", (int) FileWhence::CUR);
    INIT_OBJ("SEEK_END", (int) FileWhence::END);

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
