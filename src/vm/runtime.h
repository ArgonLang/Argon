// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <object/arobject.h>

#include "arroutine.h"
#include "context.h"

namespace argon::vm {
    argon::object::ArObject *GetLastError();

    argon::object::ArObject *Panic(argon::object::ArObject *obj);

    argon::object::ArObject *Call(argon::object::ArObject *callable, int argc, argon::object::ArObject **args);

    argon::object::ArObject *Call(argon::object::ArObject *callable, int argc, ...);

    ArRoutine *GetRoutine();

    bool Initialize();

    bool Shutdown();

    void StopTheWorld();

    void StartTheWorld();

    void STWCheckpoint();

    bool IsPanicking();

    Context *GetContext();
}

#endif //ARGON_VM_RUNTIME_H_
