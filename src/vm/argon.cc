// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>

#include <lang/compiler_wrapper.h>

#include "areval.h"
#include "argon.h"

using namespace argon::object;
using namespace argon::vm;

int argon::vm::Main(int argc, char **argv) {
    ArObject *args;

    if (!Initialize())
        return -1;

    // args = ParseCMDArgs(argc - 1, argv + 1);

    // TODO: TBD

    Shutdown();

    return 0;
}

argon::object::ArObject *argon::vm::EvalFile(const char *file) {
    lang::CompilerWrapper cw;
    ArObject *res = nullptr;
    Code *code;
    FILE *f;

    if ((f = fopen(file, "r")) == nullptr)
        return ErrorSetFromErrno();

    if ((code = cw.Compile("main", f)) != nullptr) {
        res = EvalCode(code, nullptr);
        Release(code);
    }

    fclose(f);
    return res;
}

ArObject *argon::vm::EvalString(const char *str) {
    lang::CompilerWrapper cw;
    ArObject *res = nullptr;
    Code *code;

    if ((code = cw.Compile("main", str)) != nullptr) {
        res = EvalCode(code, nullptr);
        Release(code);
    }

    return res;
}

ArObject *argon::vm::EvalCode(Code *code, Namespace *globals, Tuple *args) {
    ArObject *res = nullptr;
    Frame *frame;

    // TODO: set command args!
    if (args != nullptr) {

    }

    RoutineReset(GetRoutine(), ArRoutineStatus::RUNNING);

    if ((frame = FrameNew(code, globals, nullptr)) != nullptr)
        res = Eval(GetRoutine(), frame);

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