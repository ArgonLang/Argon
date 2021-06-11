// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <iostream>

#include <vm/argon.h>
#include <vm/config.h>

using namespace argon::object;
using namespace argon::vm;

int main(int argc, char **argv) {
    ArObject *ret;
    int err;

    if (argc < 2) {
        std::cerr << "missing script name" << std::endl;
        return -1;
    }

    err = argon::vm::ConfigInit(argc, argv);

    if (err <= 0)
        return err < 0 ? 2 : 0;

    argon::vm::Initialize();

    ImportAddPath(GetContext()->import, ".");

    if (global_cfg->cmd != -1)
        ret = argon::vm::EvalString(argv[global_cfg->cmd]);
    else
        ret = argon::vm::EvalFile(global_cfg->argv[0]);

    err = 0;
    if (!IsNull(ret)) {
        auto *str = (String *) ToString(ret);
        if (IsPanicking())
            str = (String *) ToString(GetLastError());

        std::cerr << str->buffer << std::endl;
        err = 1;
    }

    Release(ret);
    argon::vm::Shutdown();
    return err;
}

