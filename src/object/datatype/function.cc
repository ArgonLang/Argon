// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/arobject.h>
#include "function.h"

using namespace argon::object;
using namespace argon::memory;

bool function_is_true(Function *self) {
    return true;
}

bool function_equal(Function *self, ArObject *other) {
    auto *o = (Function *) other;

    if (self == other)
        return true;

    if (AR_TYPEOF(other, type_function_)) {
        if (self->native_fn == o->native_fn) {
            if (!self->native && !AR_EQUAL(self->code, o->code))
                return false;

            return self->instance == o->instance
                   && AR_EQUAL(self->currying, o->currying)
                   && AR_EQUAL(self->enclosed, o->enclosed);
        }
    }

    return false;
}

size_t function_hash(Function *self) {
    return (size_t) self;
}

ArObject *function_str(Function *self) {
    // TODO: improve this: self->name->buffer
    return StringNewFormat("<function %s at %p>", self->name->buffer, self);
}

void function_trace(Function *self, VoidUnaryOp trace) {
    trace(self->currying);
    trace(self->enclosed);
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
        "function",
        nullptr,
        sizeof(Function),
        nullptr,
        (VoidUnaryOp) function_cleanup,
        (Trace) function_trace,
        nullptr,
        (BoolBinOp) function_equal,
        (BoolUnaryOp) function_is_true,
        (SizeTUnaryOp) function_hash,
        (UnaryOp) function_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

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

Function *argon::object::FunctionNew(Namespace *gns, const NativeFunc *native) {
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
