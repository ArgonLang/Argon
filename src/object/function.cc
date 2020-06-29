// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "function.h"

using namespace argon::object;
using namespace argon::memory;

inline void CloneFn(Function *fn_new, const Function *func) {

    if (!func->native) {
        IncRef(func->code);
        fn_new->code = func->code;
    } else
        fn_new->native_fn = func->native_fn;

    IncRef(func->currying);
    fn_new->currying = func->currying;

    IncRef(func->enclosed);
    fn_new->enclosed = func->enclosed;

    IncRef(func->instance);
    fn_new->instance = func->instance;

    fn_new->arity = func->arity;
    fn_new->variadic = func->variadic;
    fn_new->native = func->native;
}

void function_cleanup(Function *fn) {
    if (!fn->native)
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
        nullptr,
        (VoidUnaryOp) function_cleanup
};

Function *argon::object::FunctionNew(Code *code, unsigned short arity, bool variadic, argon::object::List *enclosed) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
        fn->type = &type_function_;

        IncRef(code);
        fn->code = code;
        fn->currying = nullptr;

        IncRef(enclosed);
        fn->enclosed = enclosed;

        fn->instance = nullptr;

        fn->arity = arity;
        fn->variadic = variadic;
        fn->native = false;
    }

    return fn;
}

Function *argon::object::FunctionNew(NativeFuncPtr func, unsigned short arity, bool variadic) {
    auto fn = FunctionNew(nullptr, arity, variadic, nullptr);

    if (fn != nullptr) {
        fn->native_fn = func;
        fn->native = true;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, unsigned short currying_len) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
        fn->type = &type_function_;

        CloneFn(fn, func);

        if ((fn->currying = ListNew(currying_len)) == nullptr) {
            Release(fn);
            return nullptr;
        }
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, ArObject *instance) {
    auto fn = (Function *) Alloc(sizeof(Function));

    if (fn != nullptr) {
        fn->ref_count = ARGON_OBJECT_REFCOUNT_INLINE;
        fn->type = &type_function_;

        CloneFn(fn, func);

        IncRef(instance);
        fn->instance = instance;
    }

    return fn;
}
