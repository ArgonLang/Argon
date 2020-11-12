// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <object/arobject.h>
#include "ar_routine.h"
#include "context.h"

namespace argon::vm {
    bool Initialize();

    bool Shutdown();

    ArRoutine *GetRoutine();

    Context *GetContext();

    void SetRoutineMain(ArRoutine *routine);

    bool IsPanicking();

    argon::object::ArObject *GetLastError();

    argon::object::ArObject *Panic(argon::object::ArObject *obj);
}

#endif //ARGON_VM_RUNTIME_H_
