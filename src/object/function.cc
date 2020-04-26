// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "function.h"

using namespace argon::object;
using namespace argon::memory;

inline void CloneFn(Function *fn_new, const Function *func) {
    IncRef(func->code);
    fn_new->code = func->code;

    if (func->currying != nullptr) {
        IncRef(func->currying);
        fn_new->currying = func->currying;
    }

    if (func->enclosed != nullptr) {
        IncRef(func->enclosed);
        fn_new->enclosed = func->enclosed;
    }

    fn_new->arity = func->arity;
    fn_new->variadic = func->variadic;
}

void function_cleanup(Function *fn) {
    Release(fn->code);
    Release(fn->currying);
    Release(fn->enclosed);
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
        fn->enclosed = nullptr;
        fn->arity = arity;
        fn->variadic = false;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, unsigned short currying_len) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->strong_or_ref = 1;
        fn->type = &type_function_;

        CloneFn(fn, func);

        if ((fn->currying = ListNew(currying_len)) == nullptr) {
            Release(fn);
            return nullptr;
        }
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, argon::object::List *enclosed) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->strong_or_ref = 1;
        fn->type = &type_function_;

        CloneFn(fn, func);

        IncRef(enclosed);
        fn->enclosed = enclosed;
    }

    return fn;
}