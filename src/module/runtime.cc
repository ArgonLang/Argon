// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>
#include <vm/config.h>

#include <utils/macros.h>

#include <object/datatype/io/io.h>
#include <object/datatype/nil.h>
#include <object/datatype/tuple.h>

#include "modules.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::vm;

ARGON_FUNCTION5(runtime_, yield, "Give another ArRoutine a chance to run on this thread."
                                 ""
                                 "If this ArRoutine cannot be suspended and rescheduled, "
                                 "an operating system this_thread::yield call will be invoked."
                                 ""
                                 "- Returns: nil",0,false){
    SchedYield(false);
    return IncRef(NilVal);
}

ARGON_FUNCTION5(runtime_, lockthread, "Wire the currently running ArRoutine with this OS thread."
                                      ""
                                      "This call prevents another ArRoutine from running on this thread. "
                                      "If this call is invoked by the main routine, it becomes a no-op call."
                                      ""
                                      "- Returns: nil",0,false){
    LockOsThread();
    return IncRef(NilVal);
}

const PropertyBulk runtime_bulk[] = {
        MODULE_EXPORT_FUNCTION(runtime_lockthread_),
        MODULE_EXPORT_FUNCTION(runtime_yield_),
        MODULE_EXPORT_SENTINEL
};

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

    if(global_cfg->unbuffered)
        io::SetBuffer(*out, nullptr, 0, io::FileBufferMode::NONE);

    io::SetBuffer(*err, nullptr, 0, io::FileBufferMode::NONE);
    return true;
}

bool GetOS(String **name) {
    *name = StringIntern(_ARGON_PLATFORM_NAME);
    return *name != nullptr;
}

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

bool runtime_init(Module *module) {
#define ADD_PROPERTY(name, object, pinfo)               \
    if(!ModuleAddProperty(module, name, object, pinfo)) \
        goto error

    String *os_name = nullptr;
    Tuple *argv = nullptr;

    io::File *in = nullptr;
    io::File *out = nullptr;
    io::File *err = nullptr;

    bool ok = false;

    // Init IO
    if (!InitFDs(&in, &out, &err))
        goto error;

    if((argv = ParseCMDArgs(global_cfg->argc, global_cfg->argv)) == nullptr)
        goto error;

    ADD_PROPERTY("__stdin", in, MODULE_ATTRIBUTE_PUB_CONST);
    ADD_PROPERTY("__stdout", out, MODULE_ATTRIBUTE_PUB_CONST);
    ADD_PROPERTY("__stderr", err, MODULE_ATTRIBUTE_PUB_CONST);

    ADD_PROPERTY("stdin", in, PropertyType::PUBLIC);
    ADD_PROPERTY("stdout", out, PropertyType::PUBLIC);
    ADD_PROPERTY("stderr", err, PropertyType::PUBLIC);

    ADD_PROPERTY("argv", argv, MODULE_ATTRIBUTE_PUB_CONST);

    if (!GetOS(&os_name))
        goto error;

    ADD_PROPERTY("os", os_name, MODULE_ATTRIBUTE_PUB_CONST);

    ok = true;

    error:
    Release(in);
    Release(out);
    Release(err);
    Release(os_name);
    Release(argv);
    return ok;

#undef ADD_PROPERTY
}

const ModuleInit module_runtime = {
        "runtime",
        "Interact with ArgonVM. Access directly to objects used or maintained by Argon"
        "and to functions that interact strongly with it.",
        runtime_bulk,
        runtime_init,
        nullptr
};
const ModuleInit *argon::module::module_runtime_ = &module_runtime;
