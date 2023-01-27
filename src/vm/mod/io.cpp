// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/integer.h>

#include <vm/io/fio.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <stdio.h>

#else

#include <unistd.h>

#endif

#include "modules.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io;

ARGON_FUNCTION(io_open, open,
               "Open file for reading.\n"
               "\n"
               "- Parameter path: File path.\n"
               "- Returns: New File object.\n",
               "s: path", false, false) {
    return (ArObject *) FileNew((const char *) ARGON_RAW_STRING((String *) *args), FileMode::READ);
}

ARGON_FUNCTION(io_openfd, openfd,
               "Create a new File object associated with the given fd.\n"
               "\n"
               "- Parameters:\n"
               "  - fd: Int representing a file descriptor.\n"
               "  - mode: Opening mode.\n"
               "- Returns: New File object.\n",
               "i: fd, i: mode", false, false) {
    return (ArObject *) FileNew(((Integer *) *args)->sint, (FileMode) ((Integer *) args[1])->sint);
}

ARGON_FUNCTION(io_openfile, openfile,
               "Opens the named file with specified flag.\n"
               "\n"
               "- Parameters:\n"
               "  - path: File path.\n"
               "  - mode: Opening mode.\n"
               "- Returns: New File object.\n",
               "s: path, i: mode", false, false) {
    return (ArObject *) FileNew((const char *) ARGON_RAW_STRING((String *) *args),
                                (FileMode) ((Integer *) args[1])->sint);
}

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
        MODULE_EXPORT_TYPE(argon::vm::io::type_reader_t_),
        MODULE_EXPORT_TYPE(argon::vm::io::type_writer_t_),


        MODULE_EXPORT_FUNCTION(io_open),
        MODULE_EXPORT_FUNCTION(io_openfd),
        MODULE_EXPORT_FUNCTION(io_openfile),
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