// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_ARGON_VM_H_
#define ARGON_VM_ARGON_VM_H_

#include <object/object.h>
#include "ar_routine.h"

namespace argon::vm {
    class ArgonVM {
    private:
        void Eval(ArRoutine *routine);

    public:
        argon::object::ArObject *EvalCode(argon::object::Code *code);
    };
} // namespace argon::vm

#endif // !ARGON_VM_ARGON_VM_H_
