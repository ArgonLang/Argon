// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_ARGON_VM_H_
#define ARGON_VM_ARGON_VM_H_

#include <object/arobject.h>
#include <object/module.h>
#include "ar_routine.h"

namespace argon::vm {
    class ArgonVM {
    private:
        argon::object::Module *main;

    public:
        ArgonVM();

        argon::object::ArObject *EvalCode(argon::object::Code *code);
    };
} // namespace argon::vm

#endif // !ARGON_VM_ARGON_VM_H_
