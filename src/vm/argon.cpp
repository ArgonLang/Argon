// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/config.h>
#include <vm/runtime.h>

#include "argon.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

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

    if(pkgs == nullptr)
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
    Module *main;

    memory::MemoryCopy(&config, kConfigDefault, sizeof(Config));

    if (!ConfigInit(&config, argc, argv))
        return EXIT_FAILURE;

    if (!Initialize(&config))
        return EXIT_FAILURE;

    if ((context = ContextNew()) == nullptr)
        return EXIT_FAILURE;

    if (!SetupImportPaths(context->imp))
        return EXIT_FAILURE;

    if ((main = importer::ImportAdd(context->imp, "main")) == nullptr)
        return EXIT_FAILURE;

    if (config.file > -1) {
        Release(EvalFile(context, "main", argv[config.file], main->ns));

        return 0;
    }

    if (config.cmd > -1) {
        Release(EvalString(context, "main", argv[config.cmd], main->ns));

        return 0;
    }

    assert(false);
}
