// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/arobject.h>
#include "bool.h"
#include "function.h"

using namespace argon::object;
using namespace argon::memory;

bool function_is_true(Function *self) {
    return true;
}

ArObject *function_compare(Function *self, ArObject *other, CompareMode mode) {
    auto *o = (Function *) other;
    bool equal = true;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_TYPEOF(other, type_function_) || mode != CompareMode::EQ)
        return nullptr;

    if (self->native_fn == o->native_fn) {
        equal = !self->IsNative() && Equal(self->code, o->code);

        if (equal)
            equal = self->flags == o->flags && Equal(self->currying, o->currying) && Equal(self->enclosed, o->enclosed);
    }

    return BoolToArBool(equal);
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
    if (!fn->IsNative())
        Release(fn->code);

    Release(fn->name);
    Release(fn->currying);
    Release(fn->enclosed);
    Release(fn->base);
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
        (CompareOp) function_compare,
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
        nullptr,
        nullptr,
        nullptr
};


Function *CloneFn(const Function *func) {
    auto fn = ArObjectGCNew<Function>(&type_function_);

    if (fn != nullptr) {
        if (!func->IsNative())
            fn->code = IncRef(func->code);
        else
            fn->native_fn = func->native_fn;

        fn->name = IncRef(func->name);
        fn->doc = IncRef(func->doc);
        fn->currying = IncRef(func->currying);
        fn->enclosed = IncRef(func->enclosed);
        fn->base = IncRef(func->base);
        fn->gns = IncRef(func->gns);
        fn->arity = func->arity;
        fn->flags = func->flags;
    }

    return fn;
}

Function *
argon::object::FunctionNew(Namespace *gns, String *name, String *doc, Code *code, List *enclosed, unsigned short arity,
                           FunctionType flags) {
    auto fn = ArObjectGCNew<Function>(&type_function_);

    if (fn != nullptr) {
        fn->code = IncRef(code);
        fn->name = IncRef(name);
        fn->doc = IncRef(doc);
        fn->currying = nullptr;
        fn->enclosed = IncRef(enclosed);
        fn->base = nullptr;
        fn->gns = IncRef(gns);
        fn->arity = arity;
        fn->flags = flags;
    }

    return fn;
}

Function *argon::object::FunctionNew(Namespace *gns, const NativeFunc *native) {
    FunctionType flags = FunctionType::NATIVE;
    Function *fn;
    String *name;
    String *doc;

    if ((name = StringNew(native->name)) == nullptr)
        return nullptr;

    if ((doc = StringNew(native->doc)) == nullptr) {
        Release(name);
        return nullptr;
    }

    if (native->variadic)
        flags |= FunctionType::VARIADIC;

    fn = FunctionNew(gns, name, nullptr, nullptr, nullptr, native->arity, flags);
    Release(name);
    Release(doc);

    if (fn != nullptr)
        fn->native_fn = native->func;

    return fn;
}

Function *argon::object::FunctionMethodNew(Namespace *gns, TypeInfo *type, const NativeFunc *native) {
    Function *fn = FunctionNew(gns, native);

    if (fn != nullptr) {
        fn->base = IncRef(type);
        fn->flags |= FunctionType::METHOD;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, List *currying) {
    auto fn = CloneFn(func);

    if (fn != nullptr) {
        Release(fn->currying);
        fn->currying = IncRef(currying);
    }

    return fn;
}
