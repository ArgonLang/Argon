// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <vm/datatype/arobject.h>
#include <vm/datatype/code.h>
#include <vm/datatype/function.h>
#include <vm/datatype/namespace.h>
#include <vm/datatype/result.h>

#include "config.h"
#include "context.h"
#include "fiber.h"

namespace argon::vm {
    constexpr const unsigned int kOSThreadMax = 10000;
    constexpr const unsigned int kOSTStealWorkAttempts = 3;
    constexpr const unsigned short kScheduleTickBeforeCheck = 32;
    constexpr const unsigned short kVCoreDefault = 4;
    constexpr const unsigned short kVCoreQueueLengthMax = 256;

    argon::vm::datatype::ArObject *GetLastError();

    argon::vm::datatype::Future *EvalAsync(datatype::Function *func, datatype::ArObject **argv,
                                           datatype::ArSize argc, OpCodeCallMode mode);

    argon::vm::datatype::Result *Eval(Context *context, datatype::Code *code, datatype::Namespace *ns);

    argon::vm::datatype::Result *EvalFile(Context *context, const char *name,
                                          const char *path, datatype::Namespace *ns);

    argon::vm::datatype::Result *EvalString(Context *context, const char *name,
                                            const char *source, datatype::Namespace *ns);

    argon::vm::datatype::String *GetExecutableName();

    argon::vm::datatype::String *GetExecutablePath();

    bool CheckLastPanic(const char *id);

    bool Initialize(const Config *config);

    bool IsPanicking();

    bool IsPanickingFrame();

    bool Shutdown();

    bool Spawn(datatype::Function *func, datatype::ArObject **argv, datatype::ArSize argc, OpCodeCallMode mode);

    Fiber *GetFiber();

    FiberStatus GetFiberStatus();

    Frame *GetFrame();

    void Cleanup();

    void DiscardLastPanic();

    void Panic(datatype::ArObject *panic);

    void SetFiberStatus(FiberStatus status);

    void Spawn(Fiber *fiber);

    void Yield();
} // namespace argon::vm

#endif // !ARGON_VM_RUNTIME_H_
