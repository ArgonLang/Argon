// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "function.h"

using namespace argon::vm::datatype;

const TypeInfo FunctionType = {
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

Function *FunctionNew(String *name, String *doc, unsigned short arity, FunctionFlags flags) {
    auto *fn = MakeGCObject<Function>(type_function_);

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

Function *argon::vm::datatype::FunctionNew(const FunctionDef *func, TypeInfo *base) {
    FunctionFlags flags = FunctionFlags::NATIVE;
    String *name;
    String *qname;
    String *doc;

    if ((name = StringNew(func->name)) == nullptr)
        return nullptr;

    if((qname = StringFormat("%s::%s", base->qname, func->name))== nullptr){
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
        fn->native = func->func;
        fn->base = IncRef(base);
    }

    Release(name);
    Release(qname);
    Release(doc);

    return fn;
}
