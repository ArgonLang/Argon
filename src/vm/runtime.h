// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <vm/datatype/arobject.h>
#include <vm/datatype/code.h>
#include <vm/datatype/namespace.h>

#include "config.h"
#include "fiber.h"

namespace argon::vm {
    constexpr const unsigned short kVCoreDefault = 4;
    constexpr const unsigned int kOSThreadMax = 10000;

    argon::vm::datatype::ArObject *Eval(datatype::Code *code, datatype::Namespace *ns);

    argon::vm::datatype::ArObject *GetLastError();

    bool Initialize(const Config *config);

    bool IsPanicking();

    Fiber *GetFiber();

    Frame *GetFrame();

    void DiscardLastPanic();

    void Panic(datatype::ArObject *panic);
} // namespace argon::vm

#endif // !ARGON_VM_RUNTIME_H_
