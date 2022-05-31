// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <utils/macros.h>

#if defined(_ARGON_PLATFORM_WINDOWS)

#include <module/nt/nt.h>

#elif defined(_ARGON_PLATFORM_DARWIN)

#include <mach-o/dyld.h>

#else

#include <unistd.h>

#endif

#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/nil.h>
#include <object/datatype/io/io.h>
#include <object/datatype/tuple.h>

#include <vm/runtime.h>
#include <vm/config.h>
#include <vm/version.h>

#include "modules.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::vm;

Tuple *ParseCMDArgs(int argc, char **argv) {
    Tuple *args;
    String *tmp;

    if ((args = TupleNew(argc)) != nullptr) {
        for (int i = 0; i < argc; i++) {
            if ((tmp = StringNew(argv[i])) == nullptr) {
                Release(args);
                return nullptr;
            }
            TupleInsertAt(args, i, tmp);
            Release(tmp);
        }
    }

    return args;
}

bool InitFD(Module *self) {
    io::File *in;
    io::File *out;
    io::File *err = nullptr;

    bool ok = false;

    if ((in = io::FdOpen(STDIN_FILENO, io::FileMode::READ)) == nullptr)
        return false;

    if ((out = io::FdOpen(STDOUT_FILENO, io::FileMode::WRITE)) == nullptr)
        goto ERROR;

    if ((err = io::FdOpen(STDOUT_FILENO, io::FileMode::WRITE)) == nullptr)
        goto ERROR;

    io::SetBuffer(err, nullptr, 0, io::FileBufferMode::NONE);

    if (global_cfg->unbuffered)
        io::SetBuffer(out, nullptr, 0, io::FileBufferMode::NONE);

    // FDs backup
    if (!ModuleAddProperty(self, "__stdin", in, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    if (!ModuleAddProperty(self, "__stdout", out, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    if (!ModuleAddProperty(self, "__stderr", err, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    // FDs
    if (!ModuleAddProperty(self, "stdin", in, PropertyType::PUBLIC))
        goto ERROR;

    if (!ModuleAddProperty(self, "stdout", out, PropertyType::PUBLIC))
        goto ERROR;

    if (!ModuleAddProperty(self, "stderr", err, PropertyType::PUBLIC))
        goto ERROR;

    ok = true;

    ERROR:
    Release(in);
    Release(out);
    Release(err);
    return ok;
}

bool SetAbout(Module *self) {
#define ADD_INFO(macro, key)                                                \
    if ((tmp = StringNew(macro)) == nullptr)                                \
        return false;                                                       \
    if (!ModuleAddProperty(self, key, tmp, MODULE_ATTRIBUTE_PUB_CONST))   \
        goto ERROR;                                                         \
    Release(tmp)

    bool ok = false;
    ArObject *tmp;

    ADD_INFO(AR_RELEASE_LEVEL, "version_level");
    ADD_INFO(AR_VERSION, "version");
    ADD_INFO(AR_VERSION_EX, "version_ex");

    if ((tmp = IntegerNew(AR_MAJOR)) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(self, "version_major", tmp, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    Release(tmp);

    if ((tmp = IntegerNew(AR_MINOR)) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(self, "version_minor", tmp, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    Release(tmp);

    if ((tmp = IntegerNew(AR_PATCH)) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(self, "version_patch", tmp, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    Release(tmp);

    ok = true;
    tmp = nullptr;

    ERROR:
    Release(tmp);
    return ok;
#undef ADD_INFO
}

bool SetArgs(Module *self) {
    Tuple *args;

    if ((args = ParseCMDArgs(global_cfg->argc, global_cfg->argv)) == nullptr)
        return false;

    if (!ModuleAddProperty(self, "args", args, PropertyType::PUBLIC)) {
        Release(args);
        return false;
    }

    Release(args);
    return true;
}

bool SetExecutable(Module *self) {
    unsigned long size = 1024;
    String *path;
    char *path_buf;

    if ((path_buf = (char *) argon::memory::Alloc(size)) == nullptr)
        return false;

#if defined(_ARGON_PLATFORM_WINDOWS)

    size = nt::GetExecutablePath(path_buf, (int) size);

#elif defined(_ARGON_PLATFORM_LINUX)

    size = readlink("/proc/self/exe", path_buf, size);

#elif defined(_ARGON_PLATFORM_DARWIN)

    size = _NSGetExecutablePath(path_buf, (unsigned int *) &size);

#endif

    path = size != -1 ? StringNew(path_buf) : StringIntern("");
    argon::memory::Free(path_buf);

    size = 0;
    if (path != nullptr) {
        size = ModuleAddProperty(self, "executable", path, MODULE_ATTRIBUTE_PUB_CONST);
        Release(path);
    }

    return size;
}

bool SetOsName(Module *self) {
    String *name;

    if ((name = StringIntern(_ARGON_PLATFORM_NAME)) == nullptr)
        return false;

    if (!ModuleAddProperty(self, "os", name, MODULE_ATTRIBUTE_PUB_CONST)) {
        Release(name);
        return false;
    }

    Release(name);
    return true;
}

bool SetPS(Module *self) {
    bool ok = false;
    String *ps1;
    String *ps2;

    if ((ps1 = StringIntern("Ar> ")) == nullptr)
        return false;

    if ((ps2 = StringIntern("... ")) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(self, "ps1", ps1, PropertyType::PUBLIC))
        goto ERROR;

    if (!ModuleAddProperty(self, "ps2", ps2, PropertyType::PUBLIC))
        goto ERROR;

    ok = true;

    ERROR:
    Release(ps1);
    Release(ps2);
    return ok;
}

bool runtime_init(Module *self) {
    if (!InitFD(self))
        return false;

    if (!SetOsName(self))
        return false;

    if (!SetPS(self))
        return false;

    if (!SetAbout(self))
        return false;

    if (!SetArgs(self))
        return false;

    if (!SetExecutable(self))
        return false;

    return true;
}

ARGON_FUNCTION5(runtime_, exit, "Exit to the system with specified status."
                                ""
                                "- Parameter status: an integer value that defines the exit status."
                                "- Returns: this function does not return to the caller.", 1, false) {
    return argon::vm::Panic(ErrorNew(type_runtime_exit_error_, argv[0]));
}

ARGON_FUNCTION5(runtime_, lockthread, "Wire the currently running ArRoutine with this OS thread."
                                      ""
                                      "This call prevents another ArRoutine from running on this thread. "
                                      "If this call is invoked by the main routine, it becomes a no-op call."
                                      ""
                                      "- Returns: nil", 0, false) {
    LockOsThread();
    return IncRef(NilVal);
}

ARGON_FUNCTION5(runtime_, sleep, "Suspend execution of the calling ArRoutine for the given number of seconds."
                                 ""
                                 "- Parameter sec: amount of time in seconds."
                                 "- Returns: nil", 1, false) {
    const auto *integer = (Integer *) argv[0];

    if (!AR_TYPEOF(argv[0], type_integer_))
        return ErrorFormat(type_type_error_, "sleep expected sec as integer, got: %s", AR_TYPE_NAME(argv[0]));

    argon::vm::Sleep((unsigned int) integer->integer);

    return IncRef(NilVal);
}

ARGON_FUNCTION5(runtime_, usleep, "Suspend execution of the calling ArRoutine for the given number of micro-seconds."
                                  ""
                                  "- Parameter usec: amount of time in micro-seconds."
                                  "- Returns: nil", 1, false) {
    const auto *integer = (Integer *) argv[0];

    if (!AR_TYPEOF(argv[0], type_integer_))
        return ErrorFormat(type_type_error_, "usleep expected usec as integer, got: %s", AR_TYPE_NAME(argv[0]));

    argon::vm::USleep((unsigned int) integer->integer);

    return IncRef(NilVal);
}

ARGON_FUNCTION5(runtime_, sched, "Give another ArRoutine a chance to run on this thread."
                                 ""
                                 "If this ArRoutine cannot be suspended and rescheduled, "
                                 "an operating system this_thread::yield call will be invoked."
                                 ""
                                 "- Returns: nil", 0, false) {
    SchedYield(false);
    return IncRef(NilVal);
}

const PropertyBulk runtime_bulk[] = {
        MODULE_EXPORT_FUNCTION(runtime_exit_),
        MODULE_EXPORT_FUNCTION(runtime_lockthread_),
        MODULE_EXPORT_FUNCTION(runtime_sleep_),
        MODULE_EXPORT_FUNCTION(runtime_usleep_),
        MODULE_EXPORT_FUNCTION(runtime_sched_),
        MODULE_EXPORT_SENTINEL
};

const ModuleInit module_runtime = {
        "runtime",
        "Interact with ArgonVM. Access directly to objects used or maintained by Argon"
        "and to functions that interact strongly with it.",
        runtime_bulk,
        runtime_init,
        nullptr
};
const ModuleInit *argon::module::module_runtime_ = &module_runtime;
