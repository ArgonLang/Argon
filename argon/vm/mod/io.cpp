// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/pcheck.h>
#include <argon/vm/datatype/tuple.h>

#include <argon/vm/io/fio.h>
#include <argon/vm/io/pipe.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <stdio.h>

#include <argon/vm/datatype/error.h>
#include <argon/vm/support/nt/handle.h>

#else

#include <unistd.h>
#include <fcntl.h>

#endif

#include <argon/vm/mod/modules.h>

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

#ifdef _ARGON_PLATFORM_WINDOWS

ARGON_FUNCTION(io_openhandle, openhandle,
               "Create a new File object associated with the given Windows HANDLE.\n"
               "\n"
               "- Parameters:\n"
               "  - HANDLE: Handle representing a file descriptor.\n"
               "  - mode: Opening mode.\n"
               "- Returns: New File object.\n",
               ": handle, i: mode", false, false) {
    IOHandle handle;

    if (AR_TYPEOF(args[0], type_uint_))
        handle = (IOHandle) ((Integer *) args[0])->uint;
    else if (AR_TYPEOF(args[0], argon::vm::support::nt::type_oshandle_))
        handle = ((argon::vm::support::nt::OSHandle *) args[0])->handle;
    else {
        ErrorFormat(kTypeError[0], "expected '%s' or '%s, got '%s'", type_uint_->name,
                    argon::vm::support::nt::type_oshandle_->name, AR_TYPE_QNAME(args[0]));

        return nullptr;
    }

    return (ArObject *) FileNew(handle, (FileMode) ((Integer *) args[1])->sint);
}

#endif

ARGON_FUNCTION(io_mkpipe, mkpipe,
               "Creates a pipe, a unidirectional data channel that can be used for interprocess communication.\n"
               "\n"
               "- KWParameters:\n"
               "  - flags: flags to obtain different behavior.\n"
               "- Returns: Tuple containing two File objects, one for reading and one for writing.\n",
               nullptr, false, true) {
    IntegerUnderlying flags;

    if (!KParamLookupInt((Dict *) kwargs, "flags", &flags, 0))
        return nullptr;

    IOHandle read;
    IOHandle write;

    if (!MakePipe(&read, &write, (int) flags))
        return nullptr;

    auto *rfd = FileNew(read, FileMode::READ);
    if (rfd == nullptr) {
        ClosePipe(read);
        ClosePipe(write);

        return nullptr;
    }

    auto wfd = FileNew(write, FileMode::WRITE);
    if (wfd == nullptr) {
        Release(rfd);
        ClosePipe(write);
    }

    auto ret = TupleNew("oo", rfd, wfd);

    Release(rfd);
    Release(wfd);

    return (ArObject *) ret;
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
        MODULE_EXPORT_TYPE(argon::vm::io::type_line_reader_t_),
        MODULE_EXPORT_TYPE(argon::vm::io::type_reader_t_),
        MODULE_EXPORT_TYPE(argon::vm::io::type_writer_t_),


        MODULE_EXPORT_FUNCTION(io_open),
        MODULE_EXPORT_FUNCTION(io_openfd),
        MODULE_EXPORT_FUNCTION(io_openfile),
#ifdef _ARGON_PLATFORM_WINDOWS
        MODULE_EXPORT_FUNCTION(io_openhandle),
#endif
        MODULE_EXPORT_FUNCTION(io_mkpipe),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleIO = {
        "argon:io",
        "Module IO provides support to I/O primitives to read and write files.",
        nullptr,
        io_entries,
        IOInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_io_ = &ModuleIO;