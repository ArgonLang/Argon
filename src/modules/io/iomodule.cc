// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/function.h>

#include "io.h"
#include "iomodule.h"

using namespace argon::object;
using namespace argon::modules::io;

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
        MODULE_BULK_EXPORT_FUNCTION(io_openfile)
};

const ModuleInit module_io = {
        "io",
        "Module IO provides support to I/O primitives to read and write file",
        io_bulk
};

argon::object::Module *argon::modules::io::IONew() {
    return ModuleNew(&module_io);
}
