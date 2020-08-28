// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include "ar_routine.h"
#include "context.h"

namespace argon::vm {
    bool Initialize();

    bool Shutdown();

    ArRoutine *GetRoutine();

    Context *GetContext();

    void SetRoutineMain(ArRoutine *routine);
}

#endif //ARGON_VM_RUNTIME_H_
