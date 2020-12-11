// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>
#include <object/datatype/error.h>

#include "runtime.h"

using namespace argon::object;
using namespace argon::module;

bool InitFDs(io::File **in, io::File **out, io::File **err) {
    if ((*in = io::FdOpen(STDIN_FILENO, io::FileMode::READ)) == nullptr)
        return false;

    if ((*out = io::FdOpen(STDOUT_FILENO, io::FileMode::WRITE)) == nullptr) {
        Release(*in);
        return false;
    }

    if ((*err = io::FdOpen(STDERR_FILENO, io::FileMode::WRITE)) == nullptr) {
        Release(*in);
        Release(*out);
        return false;
    }

    io::SetBuffer(*err, nullptr, 0, io::FileBufferMode::NONE);

    return true;
}

bool runtime_init(Module *module) {
#define ADD_PROPERTY(name, object, pinfo)                       \
    if(!(ok=ModuleAddProperty(module, name, object, pinfo)))    \
        goto error;

    bool ok;
    io::File *in;
    io::File *out;
    io::File *err;

    // Init IO
    if (!InitFDs(&in, &out, &err))
        return false;

    ADD_PROPERTY("__stdin", in, PropertyInfo(PropertyType::PUBLIC | PropertyType::CONST));
    ADD_PROPERTY("__stdout", out, PropertyInfo(PropertyType::PUBLIC | PropertyType::CONST));
    ADD_PROPERTY("__stderr", err, PropertyInfo(PropertyType::PUBLIC | PropertyType::CONST));

    ADD_PROPERTY("stdin", in, PropertyInfo(PropertyType::PUBLIC));
    ADD_PROPERTY("stdout", out, PropertyInfo(PropertyType::PUBLIC));
    ADD_PROPERTY("stderr", err, PropertyInfo(PropertyType::PUBLIC));

    error:
    Release(in);
    Release(out);
    Release(err);
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

ArObject *argon::module::RuntimeGetProperty(const char *key, const TypeInfo *info) {
    auto str = StringIntern(key);
    ArObject *ret;

    if (str == nullptr)
        return nullptr;

    ret = NamespaceGetValue(argon::vm::GetContext()->runtime->module_ns, str, nullptr);
    Release(str);

    if (info != nullptr && ret->type != info) {
        ErrorFormat(&error_type_error, "expected '%s' found '%s'", info->name, ret->type->name);
        Release(ret);
        return nullptr;
    }

    return ret;
}
