// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <sstream>

#include <lang/compiler.h>

#include "areval.h"
#include "argon.h"

using namespace argon::object;
using namespace argon::vm;

ArObject *ParseCMDArgs(int argc, char **argv) {
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

int argon::vm::Main(int argc, char **argv) {
    ArObject *args;

    if (!Initialize())
        return -1;

    // args = ParseCMDArgs(argc - 1, argv + 1);

    // TODO: TBD

    Shutdown();

    return 0;
}

#include <iostream>

ArObject *argon::vm::EvalString(const std::string &str) {
    ArObject *res;
    Code *code;

    argon::lang::Compiler compiler;
    std::istringstream iss(str);

    code = compiler.Compile(&iss);

    res = EvalCode(code, nullptr);

    Release(code);

    return res;
}

ArObject *argon::vm::EvalCode(Code *code, Namespace *globals, Tuple *args) {
    ArObject *res = nullptr;
    Frame *frame;

    // TODO: set command args!
    if (args != nullptr) {

    }

    if ((frame = FrameNew(code, globals, nullptr)) != nullptr) {
        res = Eval(GetRoutine(), frame);
        //FrameDel(frame);
        RoutineReset(GetRoutine(), ArRoutineStatus::RUNNABLE);
    }

    return res;
}

ArObject *argon::vm::EvalCode(Code *code, Tuple *args) {
    ArObject *res = nullptr;
    Module *main;

    if ((main = ImportAddModule(GetContext()->import, "main")) != nullptr) {
        res = EvalCode(code, main->module_ns, args);
        Release(main);
    }

    return res;
}