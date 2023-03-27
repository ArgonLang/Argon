// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DEFER_H_
#define ARGON_VM_DEFER_H_

#include <argon/vm/datatype/function.h>

#include <argon/vm/opcode.h>

namespace argon::vm {
    struct Defer {
        /// Prev Defer.
        Defer *defer;

        /// Pointer to function object.
        datatype::Function *function;

        datatype::ArObject **args;

        datatype::ArSize count;

        OpCodeCallMode mode;
    };

    bool DeferPush(Defer **stack, datatype::Function *func, datatype::ArObject **args,
                   datatype::ArSize count, OpCodeCallMode mode);

    Defer *DeferNew(datatype::Function *func);

    Defer *DeferPop(Defer **stack);

} // namespace argon::vm

#endif // !ARGON_VM_DEFER_H_
