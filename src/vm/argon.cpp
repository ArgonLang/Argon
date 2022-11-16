// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/config.h>
#include <vm/runtime.h>

#include "argon.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

int argon::vm::ArgonMain(int argc, char **argv) {
    Config config{};

    memory::MemoryCopy(&config, kConfigDefault, sizeof(Config));

    if (!ConfigInit(&config, argc, argv))
        return EXIT_FAILURE;

    if (!Initialize(&config))
        return EXIT_FAILURE;

    auto *ns = NamespaceNew();
    assert(ns != nullptr);

    if (config.file > -1) {
        Release(EvalFile("main", argv[config.file], ns));

        return 0;
    }

    if (config.cmd > -1) {
        Release(EvalString("main", argv[config.cmd], ns));

        return 0;
    }

    assert(false);
}
