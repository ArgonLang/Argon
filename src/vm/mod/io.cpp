// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/io/fio.h>

#ifdef _ARGON_PLATFORM_WINDOWS
#include <stdio.h>
#else

#include <unistd.h>

#endif

#include "modules.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io;

bool IOInit(Module *self) {
#define ADD_CONSTANT(name, value)                   \
    if(!ModuleAddIntConstant(self, name, value))    \
        return false

    // FileMode
    ADD_CONSTANT("O_READ", (int) FileMode::READ);
    ADD_CONSTANT("O_WRITE", (int) FileMode::WRITE);
    ADD_CONSTANT("O_APPEND", (int) FileMode::APPEND);

    // FileWhence
    ADD_CONSTANT("SEEK_START", (int) FileWhence::START);
    ADD_CONSTANT("SEEK_CUR", (int) FileWhence::CUR);
    ADD_CONSTANT("SEEK_END", (int) FileWhence::END);

#ifdef _ARGON_PLATFORM_WINDOWS
        ADD_CONSTANT("STDIN_NO", _fileno(stdin));
        ADD_CONSTANT("STDOUT_NO", _fileno(stdout));
        ADD_CONSTANT("STDERR_NO", _fileno(stderr));
#else
    ADD_CONSTANT("STDIN_NO", STDIN_FILENO);
    ADD_CONSTANT("STDOUT_NO", STDOUT_FILENO);
    ADD_CONSTANT("STDERR_NO", STDERR_FILENO);
#endif

    return true;
}

const ModuleEntry io_entries[] = {
        MODULE_EXPORT_TYPE(argon::vm::io::type_file_),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleIO = {
        "argon:io",
        "Module IO provides support to I/O primitives to read and write files.",
        io_entries,
        IOInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_io_ = &ModuleIO;