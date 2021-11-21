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


#include <object/datatype/integer.h>
#include <object/datatype/io/io.h>
#include <object/datatype/tuple.h>
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

bool InitFD(Module *module) {
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
    if (!ModuleAddProperty(module, "__stdin", in, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    if (!ModuleAddProperty(module, "__stdout", out, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    if (!ModuleAddProperty(module, "__stderr", err, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    // FDs
    if (!ModuleAddProperty(module, "stdin", in, PropertyType::PUBLIC))
        goto ERROR;

    if (!ModuleAddProperty(module, "stdout", out, PropertyType::PUBLIC))
        goto ERROR;

    if (!ModuleAddProperty(module, "stderr", err, PropertyType::PUBLIC))
        goto ERROR;

    ok = true;

    ERROR:
    Release(in);
    Release(out);
    Release(err);
    return ok;
}

bool SetAbout(Module *module) {
#define ADD_INFO(macro, key)                                                \
    if ((tmp = StringNew(macro)) == nullptr)                                \
        return false;                                                       \
    if (!ModuleAddProperty(module, key, tmp, MODULE_ATTRIBUTE_PUB_CONST))   \
        goto ERROR;                                                         \
    Release(tmp)

    bool ok = false;
    ArObject *tmp;

    ADD_INFO(AR_RELEASE_LEVEL, "version_level");
    ADD_INFO(AR_VERSION, "version");
    ADD_INFO(AR_VERSION_EX, "version_ex");

    if ((tmp = IntegerNew(AR_MAJOR)) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(module, "version_major", tmp, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    Release(tmp);

    if ((tmp = IntegerNew(AR_MINOR)) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(module, "version_minor", tmp, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    Release(tmp);

    if ((tmp = IntegerNew(AR_PATCH)) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(module, "version_patch", tmp, MODULE_ATTRIBUTE_PUB_CONST))
        goto ERROR;

    Release(tmp);

    ok = true;
    tmp = nullptr;

    ERROR:
    Release(tmp);
    return ok;
#undef ADD_INFO
}

bool SetArgs(Module *module) {
    Tuple *args;

    if ((args = ParseCMDArgs(global_cfg->argc, global_cfg->argv)) == nullptr)
        return false;

    if (!ModuleAddProperty(module, "args", args, PropertyType::PUBLIC)) {
        Release(args);
        return false;
    }

    Release(args);
    return true;
}

bool SetExecutable(Module *module) {
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
        size = ModuleAddProperty(module, "executable", path, MODULE_ATTRIBUTE_PUB_CONST);
        Release(path);
    }

    return size;
}

bool SetOsName(Module *module) {
    String *name;

    if ((name = StringIntern(_ARGON_PLATFORM_NAME)) == nullptr)
        return false;

    if (!ModuleAddProperty(module, "os", name, MODULE_ATTRIBUTE_PUB_CONST)) {
        Release(name);
        return false;
    }

    Release(name);
    return true;
}

bool SetPS(Module *module) {
    bool ok = false;
    String *ps1;
    String *ps2;

    if ((ps1 = StringIntern("Ar> ")) == nullptr)
        return false;

    if ((ps2 = StringIntern("... ")) == nullptr)
        goto ERROR;

    if (!ModuleAddProperty(module, "ps1", ps1, PropertyType::PUBLIC))
        goto ERROR;

    if (!ModuleAddProperty(module, "ps2", ps2, PropertyType::PUBLIC))
        goto ERROR;

    ok = true;

    ERROR:
    Release(ps1);
    Release(ps2);
    return ok;
}

bool runtime_init(Module *module) {
    if (!InitFD(module))
        return false;

    if (!SetOsName(module))
        return false;

    if (!SetPS(module))
        return false;

    if (!SetAbout(module))
        return false;

    if (!SetArgs(module))
        return false;

    if (!SetExecutable(module))
        return false;

    return true;
}

const ModuleInit module_runtime = {
        "runtime",
        "Interact with ArgonVM. Access directly to objects used or maintained by Argon"
        "and to functions that interact strongly with it.",
        nullptr,
        runtime_init,
        nullptr
};
const ModuleInit *argon::module::module_runtime_ = &module_runtime;
