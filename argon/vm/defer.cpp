// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/defer.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

bool argon::vm::DeferPush(Defer **stack, datatype::Function *func, datatype::ArObject **args,
                          datatype::ArSize count, OpCodeCallMode mode) {
    ArObject **fn_args = nullptr;
    auto *defer = DeferNew(func);

    if (defer != nullptr) {
        if (count > 0) {
            if ((fn_args = (ArObject **) memory::Alloc(sizeof(void *) * count)) == nullptr) {
                memory::Free(defer);
                return false;
            }

            for (int i = 0; i < count; i++)
                fn_args[i] = IncRef(args[i]);
        }

        defer->args = fn_args;
        defer->count = count;
        defer->mode = mode;

        defer->defer = *stack;
        *stack = defer;
    }

    return defer != nullptr;
}

Defer *argon::vm::DeferNew(Function *func) {
    auto *defer = (Defer *) memory::Alloc(sizeof(Defer));

    if (defer != nullptr) {
        defer->defer = nullptr;
        defer->function = IncRef(func);
    }

    return defer;
}

Defer *argon::vm::DeferPop(Defer **stack) {
    auto *defer = *stack;

    if (defer != nullptr) {
        *stack = defer->defer;

        memory::Free(defer->args);
        memory::Free(defer);

        return *stack;
    }

    return nullptr;
}

