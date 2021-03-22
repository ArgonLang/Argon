// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <iostream>

#include <vm/argon.h>

using namespace argon::object;
using namespace argon::vm;

int main(int argc, char **argv) {
    ArObject *ret;

    if (argc < 2) {
        std::cerr << "missing script name" << std::endl;
        return -1;
    }

    argon::vm::Initialize();

    ImportAddPath(GetContext()->import, ".");

    ret = argon::vm::EvalFile(argv[1]);

    if (!IsNull(ret)) {
        auto *str = (String *) ToString(ret);
        std::cerr << "error: " << str->buffer << std::endl;
    }

    Release(ret);
    argon::vm::Shutdown();
    return 0;
}

