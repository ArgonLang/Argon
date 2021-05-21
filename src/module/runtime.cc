// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <unistd.h>
#include <object/datatype/io/io.h>

#include "runtime.h"

using namespace argon::object;
using namespace argon::module;

bool InitFDs(io::File **in, io::File **out, io::File **err) {
    if ((*in = io::FdOpen(STDIN_FILENO, io::FileMode::READ)) == nullptr)
        return false;

    if ((*out = io::FdOpen(STDOUT_FILENO, io::FileMode::WRITE)) == nullptr) {
        Release((ArObject **) in);
        return false;
    }

    if ((*err = io::FdOpen(STDERR_FILENO, io::FileMode::WRITE)) == nullptr) {
        Release((ArObject **) in);
        Release((ArObject **) out);
        return false;
    }

    io::SetBuffer(*err, nullptr, 0, io::FileBufferMode::NONE);
    return true;
}

bool GetOS(String **name) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    const char *os_name = "windows";
#elif defined(__APPLE__)
    const char *os_name = "darwin";
#elif defined(__linux__)
    const char *os_name = "linux";
#elif defined(__unix__)
    const char *os_name = "unix";
#else
    const char *os_name = "unknown";
#endif

    *name = StringIntern(os_name);
    return *name != nullptr;
}

bool runtime_init(Module *module) {
#define ADD_PROPERTY(name, object, pinfo)               \
    if(!ModuleAddProperty(module, name, object, pinfo)) \
        goto error

    String *os_name = nullptr;

    io::File *in = nullptr;
    io::File *out = nullptr;
    io::File *err = nullptr;

    bool ok = false;

    // Init IO
    if (!InitFDs(&in, &out, &err))
        return false;

    ADD_PROPERTY("__stdin", in, MODULE_ATTRIBUTE_PUB_CONST);
    ADD_PROPERTY("__stdout", out, MODULE_ATTRIBUTE_PUB_CONST);
    ADD_PROPERTY("__stderr", err, MODULE_ATTRIBUTE_PUB_CONST);

    ADD_PROPERTY("stdin", in, PropertyInfo(PropertyType::PUBLIC));
    ADD_PROPERTY("stdout", out, PropertyInfo(PropertyType::PUBLIC));
    ADD_PROPERTY("stderr", err, PropertyInfo(PropertyType::PUBLIC));

    if (!GetOS(&os_name))
        goto error;

    ADD_PROPERTY("os", os_name, MODULE_ATTRIBUTE_PUB_CONST);

    ok = true;

    error:
    Release(in);
    Release(out);
    Release(err);
    Release(os_name);
    return ok;

#undef ADD_PROPERTY
}

const ModuleInit module_runtime = {
        "runtime",
        "Interact with ArgonVM. Access directly to objects used or maintained by Argon"
        "and to functions that interact strongly with it.",
        nullptr,
        runtime_init,
        nullptr
};

Module *argon::module::RuntimeNew() {
    return ModuleNew(&module_runtime);
}
