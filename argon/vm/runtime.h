// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/namespace.h>
#include <argon/vm/datatype/result.h>

#include <argon/vm/config.h>
#include <argon/vm/context.h>
#include <argon/vm/fiber.h>
#include <argon/vm/opcode.h>

namespace argon::vm {
    constexpr const unsigned int kOSThreadMax = 10000;
    constexpr const unsigned short kScheduleTickBeforeCheck = 32;
    constexpr const unsigned short kVCoreDefault = 4;
    constexpr const unsigned short kVCoreQueueLengthMax = 256;

    argon::vm::datatype::ArObject *EvalRaiseError(datatype::Function *func, datatype::ArObject **argv,
                                                  datatype::ArSize argc, OpCodeCallMode mode);

    argon::vm::datatype::ArObject *GetLastError();

    argon::vm::datatype::Future *EvalAsync(Context *context, datatype::Function *func, datatype::ArObject **argv,
                                           datatype::ArSize argc, OpCodeCallMode mode);

    argon::vm::datatype::Result *Eval(Context *context, datatype::Code *code, datatype::Namespace *ns);

    argon::vm::datatype::Result *Eval(datatype::Function *func, datatype::ArObject **argv,
                                      datatype::ArSize argc, OpCodeCallMode mode);

    inline argon::vm::datatype::Result *
    Eval(datatype::Function *func, datatype::ArObject **argv, datatype::ArSize argc) {
        return Eval(func, argv, argc, OpCodeCallMode::FASTCALL);
    }

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

    inline argon::vm::datatype::Future *EvalAsync(datatype::Function *func, datatype::ArObject **argv,
                                                  datatype::ArSize argc, OpCodeCallMode mode) {
        return EvalAsync(GetFiber()->context, func, argv, argc, mode);
    }

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
