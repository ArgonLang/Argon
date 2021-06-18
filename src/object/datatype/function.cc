// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/arobject.h>
#include "bool.h"
#include "function.h"
#include "error.h"

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

ArSize function_hash(Function *self) {
    return (ArSize) self;
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

const TypeInfo FunctionType = {
        TYPEINFO_STATIC_INIT,
        "function",
        nullptr,
        sizeof(Function),
        TypeInfoFlags::BASE,
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
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_function_ = &FunctionType;

Function *CloneFn(const Function *func) {
    auto fn = ArObjectGCNew<Function>(type_function_);

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
                           FunctionFlags flags) {
    auto fn = ArObjectGCNew<Function>(type_function_);

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

Function *argon::object::FunctionNew(Namespace *gns, TypeInfo *base, const NativeFunc *native, bool method) {
    FunctionFlags flags = FunctionFlags::NATIVE;
    Function *fn;
    String *name;
    String *doc;

    if ((name = StringNew(native->name)) == nullptr)
        return nullptr;

    if ((doc = StringNew(native->doc)) == nullptr) {
        Release(name);
        return nullptr;
    }

    if (method)
        flags |= FunctionFlags::METHOD;

    if (native->variadic)
        flags |= FunctionFlags::VARIADIC;

    fn = FunctionNew(gns, name, doc, nullptr, nullptr, native->arity, flags);
    Release(name);
    Release(doc);

    if (fn != nullptr) {
        fn->base = IncRef(base);
        fn->native_fn = native->func;
    }

    return fn;
}

Function *argon::object::FunctionNew(const Function *func, List *currying) {
    auto fn = CloneFn(func);
    List *merged;

    if (fn == nullptr)
        return nullptr;
    
    if (fn->currying == nullptr) {
        fn->currying = IncRef(currying);
        return fn;
    }

    if ((merged = ListNew(fn->currying->len + currying->len)) == nullptr) {
        Release(fn);
        return nullptr;
    }

    ListConcat(merged, fn->currying);
    ListConcat(merged, currying);

    Release(fn->currying);
    fn->currying = merged;

    return fn;
}

ArObject *argon::object::FunctionCallNative(Function *func, ArObject **args, ArSize count) {
    ArObject *instance = nullptr;
    List *arguments = nullptr;
    ArObject *ret;

    if (count > 0 && func->IsMethod()) {
        instance = *args;

        if (!TraitIsImplemented(instance, func->base))
            return ErrorFormat(type_type_error_, "method %s::%s doesn't apply to '%s' type",
                               func->base->name, func->name->buffer, AR_TYPE_NAME(instance));

        args++;
        count--;
    }

    if (func->arity > 0) {
        if (func->currying != nullptr) {
            if (args != nullptr && count > 0) {
                if ((arguments = ListNew(func->currying->len + count)) == nullptr)
                    return nullptr;

                ListConcat(arguments, func->currying);

                for (ArSize i = 0; i < count; i++)
                    ListAppend(arguments, args[i]);

                args = arguments->objects;
                count = arguments->len;
            } else {
                args = func->currying->objects;
                count = func->currying->len;
            }
        }
    }

    ret = func->native_fn(func, instance, args, count);
    Release(arguments);

    return ret;
}
