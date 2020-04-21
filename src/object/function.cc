// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "function.h"

using namespace argon::object;
using namespace argon::memory;

void function_cleanup(Function *fn) {
    Release(fn->code);
    Release(fn->currying);
}

const TypeInfo argon::object::type_function_ = {
        (const unsigned char *) "function",
        sizeof(Function),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (VoidUnaryOp) function_cleanup
};

Function *argon::object::FunctionNew(Code *code, unsigned short arity) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->strong_or_ref = 1;
        fn->type = &type_function_;

        IncRef(code);
        fn->code = code;
        fn->currying = nullptr;
        fn->arity = arity;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, unsigned short currying_len) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->strong_or_ref = 1;
        fn->type = &type_function_;

        IncRef(func->code);
        fn->code = func->code;

        if ((fn->currying = ListNew(currying_len)) == nullptr) {
            Release(fn);
            return nullptr;
        }

        fn->arity = func->arity;
        fn->variadic = func->variadic;
    }

    return fn;
}