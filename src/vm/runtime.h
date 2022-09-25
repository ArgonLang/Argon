// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RUNTIME_H_
#define ARGON_VM_RUNTIME_H_

#include "datatype/arobject.h"

namespace argon::vm {
    argon::vm::datatype::ArObject *GetLastError();

    bool Initialize();

    bool IsPanicking();

    void Panic(datatype::ArObject *panic);
}

#endif // !ARGON_VM_RUNTIME_H_
