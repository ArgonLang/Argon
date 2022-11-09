// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include <vm/datatype/arobject.h>

#include "config.h"

namespace argon::vm {
    constexpr const unsigned short kVCoreDefault = 4;

    argon::vm::datatype::ArObject *GetLastError();

    bool Initialize(const Config *config);

    bool IsPanicking();

    Fiber *GetFiber();

    Frame *GetFrame();

    void DiscardLastPanic();

    void Panic(datatype::ArObject *panic);
}

#endif // !ARGON_VM_RUNTIME_H_
