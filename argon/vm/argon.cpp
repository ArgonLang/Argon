// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <iostream>

#include <argon/vm/config.h>
#include <argon/vm/runtime.h>
#include <argon/vm/signal.h>

#include <argon/vm/argon.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

// Prototypes

void PrintRaw(ArObject *);

// EOL

bool SetupImportPaths(importer::Import *imp) {
    const char *arpackages = _ARGON_PLATFORM_PATHSEP"packages";
    const char *arpaths = std::getenv(ARGON_EVAR_PATH);
    List *paths;
    String *tmp;
    String *pkgs;

    if ((tmp = GetExecutablePath()) == nullptr)
        return false;

    if (!importer::ImportAddPath(imp, tmp)) {
        Release(tmp);
        return false;
    }

    pkgs = StringConcat(tmp, arpackages, strlen(arpackages));

    Release(tmp);

    if (pkgs == nullptr)
        return false;

    if (!importer::ImportAddPath(imp, pkgs)) {
        Release(pkgs);
        return false;
    }

    Release(pkgs);

    // *** PATHS FROM ENVS VAR ***

    if (arpaths == nullptr)
        return true;

    if ((tmp = StringNew(arpaths)) == nullptr)
        return false;

    paths = (List *) StringSplit(tmp, ARGON_RAW_STRING(imp->path_sep), ARGON_RAW_STRING_LENGTH(imp->path_sep), -1);

    Release(tmp);

    if (paths == nullptr)
        return false;

    if (!importer::ImportAddPaths(imp, paths)) {
        Release(paths);
        return false;
    }

    Release(paths);

    return true;
}

int argon::vm::ArgonMain(int argc, char **argv) {
    Config config{};
    Context *context;
    Module *mod;
    Result *result;

    memory::MemoryCopy(&config, kConfigDefault, sizeof(Config));

    if (!ConfigInit(&config, argc, argv))
        return EXIT_FAILURE;

    if (!Initialize(&config))
        return EXIT_FAILURE;

    if ((context = ContextNew(&config)) == nullptr)
        return EXIT_FAILURE;

    if(!SignalInit(context))
        return EXIT_FAILURE;

    if (!SetupImportPaths(context->imp))
        return EXIT_FAILURE;

    if ((mod = importer::ImportAdd(context->imp, "__main")) == nullptr)
        return EXIT_FAILURE;

    if (config.file > -1) {
        result = EvalFile(context, "__main", argv[config.file], mod->ns);
        if (result != nullptr) {
            if (!result->success)
                PrintRaw(result->value);

            Release(result);
        } else
            PrintRaw(nullptr);
    }

    if (config.cmd > -1) {
        result = EvalString(context, "__main", argv[config.cmd], mod->ns);
        if (result != nullptr) {
            if (!result->success)
                PrintRaw(result->value);

            Release(result);
        } else
            PrintRaw(nullptr);
    }

    if (config.interactive) {
        if ((mod = importer::LoadModule(context->imp, "repl", nullptr)) == nullptr) {
            if (CheckLastPanic(kModuleImportError[0])) {
                std::cerr << "No REPL script found, interactive mode not available.\n"
                             "Check your installation!" << std::endl;
                return EXIT_FAILURE;
            }

            PrintRaw(GetLastError());
            return EXIT_FAILURE;
        }

        result = EvalString(context, "repl", "RunDefaultRepl()", mod->ns);
        if (!result->success)
            PrintRaw(result->value);

        Release(result);
    }

    if (Shutdown()) {
        ContextDel(context);
        Cleanup();
    }

    return EXIT_SUCCESS;
}

void PrintRaw(ArObject *object) {
    String *str;

    IncRef(object);

    if (object == nullptr)
        object = GetLastError();

    assert(object != nullptr);

    if(AR_TYPEOF(object, type_error_) && AtomCompareID(((Error*)object)->id, kRuntimeExitError[0]))
        return;

    str = (String *) Str(object);

    Release(object);

    if (str != nullptr) {
        if (AR_TYPEOF(object, type_error_))
            std::cerr << ARGON_RAW_STRING(str) << std::endl;
        else
            std::cout << ARGON_RAW_STRING(str) << std::endl;

        Release(str);
        return;
    }

    auto *err = GetLastError();
    str = (String *) Str(err);

    if (str != nullptr) {
        std::cerr << ARGON_RAW_STRING(str) << std::endl;
        Release(str);
        Release(err);
        return;
    }

    Release(err);
    std::cerr << "FATAL: Too many errors occurred while trying to print an object";
}
