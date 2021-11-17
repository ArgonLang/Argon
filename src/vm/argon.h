// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_ARGON_H_
#define ARGON_ARGON_H_

#include <object/arobject.h>

#include "runtime.h"
#include "version.h"

namespace argon::vm {

    int Main(int argc, char **argv);

    argon::object::ArObject *EvalFile(const char *file);

    argon::object::ArObject *EvalString(const char *str);

    argon::object::ArObject *EvalCode(argon::object::Code *code, argon::object::Namespace *globals);

    argon::object::ArObject *EvalCode(argon::object::Code *code);

    void EvalInteractive();

    int StartInteractiveLoop();

} // namespace argon

#endif // !ARGON_ARGON_H_
