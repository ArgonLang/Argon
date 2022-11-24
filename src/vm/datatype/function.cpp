// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "error.h"
#include "function.h"

using namespace argon::vm::datatype;

TypeInfo FunctionType = {
        AROBJ_HEAD_INIT_TYPE,
        "Function",
        nullptr,
        nullptr,
        sizeof(Function),
        TypeInfoFlags::BASE,
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
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_function_ = &FunctionType;

Function *FunctionClone(const Function *func) {
    auto *fn = MakeGCObject<Function>(type_function_, false);

    if (fn != nullptr) {
        fn->native = func->native;

        fn->name = IncRef(func->name);
        fn->qname = IncRef(func->qname);
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

Function *FunctionNew(String *name, String *doc, unsigned short arity, FunctionFlags flags) {
    auto *fn = MakeGCObject<Function>(type_function_, false);

    if (fn != nullptr) {
        fn->name = IncRef(name);
        fn->qname = nullptr;
        fn->doc = IncRef(doc);
        fn->currying = nullptr;
        fn->enclosed = nullptr;
        fn->base = nullptr;
        fn->gns = nullptr;
        fn->arity = arity;
        fn->flags = flags;
    }

    return fn;
}

ArObject *argon::vm::datatype::FunctionInvokeNative(Function *func, ArObject **args, ArSize count, bool kwargs) {
    ArObject **f_args = args;
    ArObject *f_kwargs = nullptr;
    ArObject *instance = nullptr;
    ArObject *ret = nullptr;

    ArSize f_count = count;

    bool free_args = false;

    // Check stub
    if (func->native == nullptr) {
        ErrorFormat(kNotImplementedError[0], kNotImplementedError[1], ARGON_RAW_STRING(func->name));
        return nullptr;
    }

    if (func->currying) {
        auto clen = func->currying->length;

        f_args = func->currying->objects;
        f_count += func->currying->length;

        if (count > 0) {
            if ((args = (ArObject **) memory::Alloc(sizeof(void *) * f_count)) == nullptr)
                return nullptr;

            memory::MemoryCopy(f_args, func->currying->objects, clen);
            memory::MemoryCopy(f_args + clen, args, count);

            free_args = true;
        }
    }

    if (f_count > 0 && func->IsMethod()) {
        instance = *f_args;

        if (!TraitIsImplemented(instance, func->base)) {
            ErrorFormat(kTypeError[0], kTypeError[5], ARGON_RAW_STRING(func->qname), AR_TYPE_NAME(instance));
            goto ERROR;
        }

        f_args++;
        f_count--;
    }

    if (kwargs && func->IsKWArgs()) {
        f_kwargs = f_args[f_count - 1];
        f_count--;
    }

    ret = func->native((ArObject *) func, instance, f_args, f_kwargs, f_count);

    ERROR:
    if (free_args)
        memory::Free(f_args);

    return ret;
}

Function *argon::vm::datatype::FunctionNew(Code *code, String *name, String *qname, Namespace *ns,
                                           List *enclosed, unsigned short arity, FunctionFlags flags) {
    auto *fn = ::FunctionNew(name, nullptr, arity, flags);

    if (fn != nullptr) {
        fn->qname = IncRef(qname);
        fn->enclosed = IncRef(enclosed);
        fn->gns = IncRef(ns);
        fn->code = IncRef(code);
    }

    return fn;
}

Function *argon::vm::datatype::FunctionNew(const Function *func, ArObject **args, ArSize nargs) {
    auto *fn = FunctionClone(func);
    if (fn == nullptr)
        return nullptr;

    auto total_args = nargs;

    if (fn->currying != nullptr)
        total_args += fn->currying->length;

    auto *list = ListNew(total_args);
    if (list == nullptr) {
        Release(fn);
        return nullptr;
    }

    ListExtend(list, (ArObject *) fn->currying);
    ListExtend(list, args, nargs);

    // Convert to Tuple:
    auto *tuple = TupleConvertList(&list);

    Release(list);

    return fn;
}

Function *argon::vm::datatype::FunctionNew(const FunctionDef *func, TypeInfo *base, Namespace *ns) {
    FunctionFlags flags = FunctionFlags::NATIVE;
    String *name;
    String *qname;
    String *doc;

    if ((name = StringNew(func->name)) == nullptr)
        return nullptr;

    if ((qname = StringFormat("%s::%s", base->qname, func->name)) == nullptr) {
        Release(name);
        return nullptr;
    }

    if ((doc = StringNew(func->doc)) == nullptr) {
        Release(name);
        Release(qname);
        return nullptr;
    }

    if (func->method)
        flags |= FunctionFlags::METHOD;

    if (func->variadic)
        flags |= FunctionFlags::VARIADIC;

    if (func->kwarg)
        flags |= FunctionFlags::KWARGS;

    auto *fn = ::FunctionNew(name, doc, func->arity, flags);

    if (fn != nullptr) {
        fn->qname = qname;
        fn->native = func->func;
        fn->base = IncRef(base);
        fn->gns = IncRef(ns);
    }

    Release(name);
    Release(qname);
    Release(doc);

    return fn;
}
