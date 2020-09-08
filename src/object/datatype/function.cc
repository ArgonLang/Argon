// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/objmgmt.h>
#include "function.h"

using namespace argon::object;
using namespace argon::memory;

Function *CloneFn(const Function *func) {
    auto fn = ArObjectNew<Function>(RCType::INLINE, &type_function_);

    if (fn != nullptr) {
        if (!func->native) {
            IncRef(func->code);
            fn->code = func->code;
        } else
            fn->native_fn = func->native_fn;

        IncRef(func->currying);
        fn->currying = func->currying;

        IncRef(func->enclosed);
        fn->enclosed = func->enclosed;

        IncRef(func->instance);
        fn->instance = func->instance;

        fn->arity = func->arity;
        fn->variadic = func->variadic;
        fn->native = func->native;
    }

    return fn;
}

bool function_equal(ArObject *self, ArObject *other) {
    if (self->type != other->type)
        return false;

    if (self != other) {
        auto l = (Function *) self;
        auto r = (Function *) other;

        if (l->native_fn == r->native_fn) {
            if (!l->native && !l->code->type->equal(l->code, r->code))
                return false;

            return l->instance == r->instance &&
                   l->currying->type->equal(l->currying, r->currying);
        }

        return false;
    }
    return true;
}

size_t function_hash(Function *self) {
    return (size_t) self;
}

void function_cleanup(Function *fn) {
    Release(fn->code);
    Release(fn->currying);
    Release(fn->enclosed);
    Release(fn->instance);
}

const TypeInfo argon::object::type_function_ = {
        (const unsigned char *) "function",
        sizeof(Function),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        function_equal,
        nullptr,
        (SizeTUnaryOp) function_hash,
        nullptr,
        nullptr,
        (VoidUnaryOp) function_cleanup
};

Function *argon::object::FunctionNew(Code *code, unsigned short arity, bool variadic, List *enclosed) {
    auto fn = ArObjectNew<Function>(RCType::INLINE, &type_function_);

    if (fn != nullptr) {
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

Function *argon::object::FunctionNew(const FunctionNative *native) {
    auto fn = FunctionNew(nullptr, native->arity, native->variadic, nullptr);

    if (fn != nullptr) {
        fn->native_fn = native->func;
        fn->native = true;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, List *currying) {
    auto fn = CloneFn(func);

    if (fn != nullptr) {
        Release(fn->currying);
        IncRef(currying);
        fn->currying = currying;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, ArObject *instance) {
    auto fn = CloneFn(func);

    if (fn != nullptr) {
        Release(fn->instance); // Actually I expect it to be nullptr!
        IncRef(instance);
        fn->instance = instance;
    }

    return fn;
}
