// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/arobject.h>
#include "function.h"

using namespace argon::object;
using namespace argon::memory;

Function *CloneFn(const Function *func) {
    auto fn = ArObjectGCNew<Function>(&type_function_);

    if (fn != nullptr) {
        if (!func->native) {
            IncRef(func->code);
            fn->code = func->code;
        } else
            fn->native_fn = func->native_fn;

        IncRef(func->name);
        fn->name = func->name;

        IncRef(func->currying);
        fn->currying = func->currying;

        IncRef(func->enclosed);
        fn->enclosed = func->enclosed;

        IncRef(func->instance);
        fn->instance = func->instance;

        IncRef(func->gns);
        fn->gns = func->gns;

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

void function_trace(Function *self, VoidUnaryOp trace) {
    trace(self->gns);
}

void function_cleanup(Function *fn) {
    Release(fn->code);
    Release(fn->name);
    Release(fn->currying);
    Release(fn->enclosed);
    Release(fn->instance);
    Release(fn->gns);
}

const TypeInfo argon::object::type_function_ = {
        TYPEINFO_STATIC_INIT,
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
        (Trace) function_trace,
        (VoidUnaryOp) function_cleanup
};

Function *argon::object::FunctionNew(Namespace *gns, String *name, Code *code, unsigned short arity, bool variadic,
                                     List *enclosed) {
    auto fn = ArObjectGCNew<Function>(&type_function_);

    if (fn != nullptr) {
        IncRef(code);
        fn->code = code;

        IncRef(name);
        fn->name = name;
        fn->currying = nullptr;

        IncRef(enclosed);
        fn->enclosed = enclosed;

        fn->instance = nullptr;

        IncRef(gns);
        fn->gns = gns;

        fn->arity = arity;
        fn->variadic = variadic;
        fn->native = false;
    }

    return fn;
}

Function *argon::object::FunctionNew(Namespace *gns, const FunctionNative *native) {
    Function *fn;
    String *name;

    if ((name = StringNew(native->name)) == nullptr)
        return nullptr;

    fn = FunctionNew(gns, name, nullptr, native->arity, native->variadic, nullptr);
    Release(name);

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
