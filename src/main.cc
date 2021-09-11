// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <iostream>

#include <vm/argon.h>
#include <vm/config.h>

using namespace argon::object;
using namespace argon::vm;

#include <lang/scanner/scanner2.h>

using namespace argon::lang::scanner2;

int main(int argc, char **argv) {
    ArObject *ret;
    int err;

    Scanner scn(stdin, ">>", "..");
    Token tok = Token(TokenType::ERROR,0,0);


    for(int i=0;i<30;i++)
        tok = scn.NextToken();

    if (argc < 2) {
        std::cerr << "missing script name" << std::endl;
        return -1;
    }

    err = argon::vm::ConfigInit(argc, argv);

    if (err <= 0)
        return err < 0 ? 2 : 0;

    argon::vm::Initialize();

    ImportAddPath(GetContext()->import, ".");

    AcquireMain();

    if (global_cfg->cmd != -1)
        ret = argon::vm::EvalString(argv[global_cfg->cmd]);
    else
        ret = argon::vm::EvalFile(global_cfg->argv[0]);

    err = 0;

    if (IsPanicking()) {
        auto *str = (String *) ToString(GetLastError());
        if (IsPanicking())
            str = (String *) ToString(GetLastError());

        std::cerr << str->buffer << std::endl;
        err = 1;
    }

    Release(ret);

    ReleaseMain();

    argon::vm::Shutdown();
    return err;
}

