// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/config.h>
#include <argon/vm/runtime.h>
#include <argon/vm/version.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/tuple.h>

using namespace argon::vm::datatype;

// Prototypes

Tuple *ParseCMDArgs(int argc, char **argv);

// EOL

bool ExposeConfig(Module *self) {
#define PUT_BOOL(name, field)                                               \
     if (!DictInsert(ret, #name, (ArObject *) (field ? True : False))) {    \
        Release(ret);                                                       \
        return false; }

#define PUT_INT(name, field)                                                \
     if((tmp = (ArObject *) IntNew(field)) == nullptr) {                    \
        Release(ret);                                                       \
        return false; }                                                     \
                                                                            \
     if (!DictInsert(ret, #name, tmp)) {                                    \
        Release(tmp);                                                       \
        Release(ret);                                                       \
        return false; }                                                     \
                                                                            \
    Release(tmp);

#define PUT_STR(name, field)                                                \
     if((tmp = (ArObject *) StringNew(field)) == nullptr) {                 \
        Release(ret);                                                       \
        return false; }                                                     \
                                                                            \
     if (!DictInsert(ret, #name, tmp)) {                                    \
        Release(tmp);                                                       \
        Release(ret);                                                       \
        return false; }                                                     \
                                                                            \
    Release(tmp);

    auto *conf = argon::vm::GetFiber()->context->global_config;

    auto *ret = DictNew();
    if (ret == nullptr)
        return false;

    ArObject *tmp;

    PUT_BOOL(interactive, conf->interactive)
    PUT_BOOL(nogc, conf->nogc)
    PUT_BOOL(quiet, conf->quiet)
    PUT_BOOL(stack_trace, conf->stack_trace)
    PUT_BOOL(unbuffered, conf->unbuffered)

    PUT_INT(max_vc, conf->max_vc)
    PUT_INT(max_ost, conf->max_ost)
    PUT_INT(fiber_ss, conf->fiber_ss)
    PUT_INT(fiber_pool, conf->fiber_pool)
    PUT_INT(optim_lvl, conf->optim_lvl)

    if (!ModuleAddObject(self, "config", (ArObject *) ret, MODULE_ATTRIBUTE_DEFAULT)) {
        Release(ret);

        return false;
    }

    Release(ret);

    return true;
#undef PUT_BOOL
#undef PUT_INT
#undef PUT_STR
}

bool SetAbout(Module *self) {
#define ADD_INFO(macro, key)                                        \
    if ((tmp = (ArObject*) StringNew(macro)) == nullptr)            \
        return false;                                               \
    if (!ModuleAddObject(self, key, tmp, MODULE_ATTRIBUTE_DEFAULT)) \
        goto ERROR;                                                 \
    Release(tmp)

    bool ok = false;
    ArObject *tmp;

    ADD_INFO(AR_RELEASE_LEVEL, "version_level");
    ADD_INFO(AR_VERSION, "version");
    ADD_INFO(AR_VERSION_EX, "version_ex");

    if ((tmp = (ArObject *) IntNew(AR_MAJOR)) == nullptr)
        goto ERROR;

    if (!ModuleAddObject(self, "version_major", tmp, MODULE_ATTRIBUTE_DEFAULT))
        goto ERROR;

    Release(tmp);

    if ((tmp = (ArObject *) IntNew(AR_MINOR)) == nullptr)
        goto ERROR;

    if (!ModuleAddObject(self, "version_minor", tmp, MODULE_ATTRIBUTE_DEFAULT))
        goto ERROR;

    Release(tmp);

    if ((tmp = (ArObject *) IntNew(AR_PATCH)) == nullptr)
        goto ERROR;

    if (!ModuleAddObject(self, "version_patch", tmp, MODULE_ATTRIBUTE_DEFAULT))
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
    const argon::vm::Config *config;
    Tuple *args;

    config = argon::vm::GetFiber()->context->global_config;

    if ((args = ParseCMDArgs(config->argc, config->argv)) == nullptr)
        return false;

    if (!ModuleAddObject(self, "args", (ArObject *) args, AttributeFlag::PUBLIC)) {
        Release(args);
        return false;
    }

    Release(args);
    return true;
}

bool SetExecutable(Module *self) {
    auto *executable = argon::vm::GetExecutableName();
    if (executable == nullptr)
        return false;

    if (!ModuleAddObject(self, "executable", (ArObject *) executable, MODULE_ATTRIBUTE_DEFAULT)) {
        Release(executable);
        return false;
    }

    Release(executable);

    return true;
}

bool SetOsName(Module *self) {
    String *name;

    if ((name = StringIntern(_ARGON_PLATFORM_NAME)) == nullptr)
        return false;

    if (!ModuleAddObject(self, "os", (ArObject *) name, MODULE_ATTRIBUTE_DEFAULT)) {
        Release(name);
        return false;
    }

    Release(name);
    return true;
}

bool RuntimeInit(Module *self) {
    if (!ExposeConfig(self))
        return false;

    if (!SetAbout(self))
        return false;

    if (!SetArgs(self))
        return false;

    if (!SetExecutable(self))
        return false;

    if (!SetOsName(self))
        return false;

    return true;
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
            TupleInsert(args, (ArObject *) tmp, i);
            Release(tmp);
        }
    }

    return args;
}

const ModuleEntry runtime_entries[] = {
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleRuntime = {
        "runtime",
        "Interact with ArgonVM. Access directly to objects used or maintained by Argon "
        "and to functions that interact strongly with it.",
        nullptr,
        runtime_entries,
        RuntimeInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_runtime_ = &ModuleRuntime;