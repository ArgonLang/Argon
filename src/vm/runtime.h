// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <object/arobject.h>

#include "arroutine.h"
#include "routinequeue.h"
#include "context.h"

namespace argon::vm {
    argon::object::ArObject *GetLastError();

    argon::object::ArObject *Panic(argon::object::ArObject *obj);

    argon::object::ArObject *Call(argon::object::ArObject *callable, int argc, argon::object::ArObject **args);

    argon::object::ArObject *Call(argon::object::ArObject *callable, int argc, ...);

    ArRoutine *GetRoutine();

    ArRoutine *UnschedRoutine(bool resume_last);

    Context *GetContext();

    bool AcquireMain();

    bool CanSpin();

    bool DiscardErrorType(const argon::object::TypeInfo *type);

    bool Initialize();

    bool IsPanicking();

    bool SchedYield(bool resume_last);

    bool SchedYield(bool resume_last, ArRoutine *routine);

    bool Spawn(ArRoutine *routine);

    bool Spawns(ArRoutine *routines);

    bool Shutdown();

    void LockOsThread();

    void ReleaseMain();

    void ReleaseQueue();

    void StartTheWorld();

    void StopTheWorld();

    void STWCheckpoint();
}

#endif //ARGON_VM_RUNTIME_H_
