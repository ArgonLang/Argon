// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <sstream>

#include <lang/compiler.h>
#include "frame.h"
#include "ar_routine.h"
#include "runtime.h"
#include "areval.h"

#include "context.h"

using namespace argon::lang;
using namespace argon::vm;
using namespace argon::object;

Context::Context() {
    // TODO: STUB
    this->main = ModuleNew("main", "main module");
    this->import = ImportNew();
    this->bltins = ImportModule(this->import, "builtins", nullptr);

    this->stdout = argon::modules::io::FdOpen(fileno(::stdout), argon::modules::io::FileMode::WRITE);
    argon::modules::io::SetBuffer(this->stdout, nullptr, 0, argon::modules::io::FileBufferMode::LINE);
}

ArObject *Context::Eval(const std::string &source) {
    auto src = std::istringstream(source);

    Compiler compiler;
    auto compiled = compiler.Compile(&src);

    Frame *frame = FrameNew(compiled, this->main->module_ns, nullptr);
    auto routine = RoutineNew(frame);

    routine->context = this;

    SetRoutineMain(routine);

    auto ret = argon::vm::Eval(routine, frame);

    SetRoutineMain(nullptr);

    RoutineDel(routine);

    return ret;
}
