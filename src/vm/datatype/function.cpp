// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"
#include "error.h"
#include "function.h"

using namespace argon::vm::datatype;

const MemberDef function_members[] = {
        ARGON_MEMBER("arity", MemberType::SHORT, offsetof(Function, arity), true),
        ARGON_MEMBER("base", MemberType::OBJECT, offsetof(Function, base), true),
        ARGON_MEMBER("doc", MemberType::OBJECT, offsetof(Function, doc), true),
        ARGON_MEMBER("name", MemberType::OBJECT, offsetof(Function, name), true),
        ARGON_MEMBER("qname", MemberType::OBJECT, offsetof(Function, qname), true),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots function_objslot = {
        nullptr,
        function_members,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *function_compare(const Function *self, const ArObject *other, CompareMode mode) {
    auto *o = (const Function *) other;
    bool equal = true;

    if (self == o && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_TYPEOF(other, type_function_) || mode != CompareMode::EQ)
        return nullptr;

    if (self->native == o->native) {
        equal = !self->IsNative() && Equal((ArObject *) self->code, (ArObject *) o->code);

        if (equal)
            equal = self->flags == o->flags
                    && Equal((ArObject *) self->currying, (ArObject *) o->currying)
                    && Equal((ArObject *) self->enclosed, (ArObject *) o->enclosed);
    }

    return BoolToArBool(equal);
}

ArObject *function_repr(const Function *self) {
    if (self->IsGenerator()) {
        return (ArObject *) StringFormat("<generator function %s at %p>", AR_TYPE_QNAME(self), self);
    }

    if (self->IsNative())
        return (ArObject *) StringFormat("<native function %s at %p>", AR_TYPE_QNAME(self), self);

    return (ArObject *) StringFormat("<function %s at %p>", AR_TYPE_QNAME(self), self);
}

ArSize function_hash(const ArObject *self) {
    return (ArSize) self;
}

bool function_dtor(Function *self) {
    if (!self->IsNative())
        Release(self->code);

    Release(self->name);
    Release(self->qname);
    Release(self->doc);
    Release(self->pcheck);
    Release(self->currying);
    Release(self->enclosed);
    Release(self->base);
    Release(self->gns);

    return true;
}

void function_trace(Function *self, Void_UnaryOp trace) {
    trace((ArObject *) self->gns);
}

TypeInfo FunctionType = {
        AROBJ_HEAD_INIT_TYPE,
        "Function",
        nullptr,
        nullptr,
        sizeof(Function),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) function_dtor,
        (TraceOp) function_trace,
        function_hash,
        nullptr,
        (CompareOp) function_compare,
        (UnaryConstOp) function_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &function_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_function_ = &FunctionType;

bool FunctionCheckParams(const PCheck *pcheck, ArObject **args, ArSize count) {
    if (pcheck == nullptr)
        return true;

    assert(pcheck->count == count);

    for (int i = 0; i < count; i++) {
        const auto *param = pcheck->params[i];
        const auto *arg = args[i];
        bool ok = false;

        if (*param->types == nullptr)
            continue;

        for (auto cursor = param->types; *cursor != nullptr; cursor++) {
            if (AR_TYPEOF(arg, *cursor)) {
                ok = true;
                break;
            }
        }

        if (!ok) {
            ErrorFormat(kTypeError[0],
                        "unexpected '%s' type for '%s' parameter(%d)",
                        AR_TYPE_NAME(arg), param->name, i);
            return false;
        }
    }

    return true;
}

Function *FunctionClone(const Function *func) {
    auto *fn = MakeGCObject<Function>(type_function_, false);

    if (fn != nullptr) {
        fn->native = func->native;

        fn->name = IncRef(func->name);
        fn->qname = IncRef(func->qname);
        fn->doc = IncRef(func->doc);
        fn->pcheck = IncRef(func->pcheck);
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
        fn->pcheck = nullptr;
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
    ArObject **f_args_base = nullptr;
    ArObject *f_kwargs = nullptr;
    ArObject *instance = nullptr;
    ArObject *ret = nullptr;

    ArSize f_count = count;

    // Check stub
    if (func->native == nullptr) {
        ErrorFormat(kNotImplementedError[0], kNotImplementedError[1], ARGON_RAW_STRING(func->qname));
        return nullptr;
    }

    if (func->currying) {
        auto clen = func->currying->length;

        f_args = func->currying->objects;
        f_count += func->currying->length;

        if (count > 0) {
            if ((f_args_base = (ArObject **) memory::Alloc(sizeof(void *) * f_count)) == nullptr)
                return nullptr;

            f_args = f_args_base;

            for (int i = 0; i < clen; i++)
                f_args[i] = func->currying->objects[i];

            for (int i = 0; i < count; i++)
                f_args[clen + i] = args[i];
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

    if (FunctionCheckParams(func->pcheck, f_args, f_count))
        ret = func->native((ArObject *) func, instance, f_args, f_kwargs, f_count);

    ERROR:
    if (f_args_base != nullptr)
        memory::Free(f_args_base);

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

    if (tuple == nullptr) {
        Release(fn);
        return nullptr;
    }

    fn->currying = tuple;

    return fn;
}

Function *argon::vm::datatype::FunctionNew(const FunctionDef *func, TypeInfo *base, Namespace *ns) {
    FunctionFlags flags = FunctionFlags::NATIVE;
    unsigned short arity = 0;
    PCheck *pcheck = nullptr;
    String *doc = nullptr;
    String *name;
    String *qname;

    if ((name = StringNew(func->name)) == nullptr)
        return nullptr;

    if ((qname = StringFormat("%s::%s", base != nullptr ? base->qname : "", func->name)) == nullptr) {
        Release(name);
        return nullptr;
    }

    if (func->doc != nullptr) {
        doc = StringNew(func->doc);
        if (doc == nullptr) {
            Release(name);
            Release(qname);
            return nullptr;
        }
    }

    if (func->params != nullptr && *func->params != '\0') {
        pcheck = PCheckNew(func->params);
        if (pcheck == nullptr) {
            Release(name);
            Release(qname);
            return nullptr;
        }

        arity = pcheck->count;
    }

    if (func->method) {
        flags |= FunctionFlags::METHOD;
        arity++;
    }

    if (func->variadic)
        flags |= FunctionFlags::VARIADIC;

    if (func->kwarg)
        flags |= FunctionFlags::KWARGS;

    auto *fn = ::FunctionNew(name, doc, arity, flags);
    if (fn != nullptr) {
        fn->qname = IncRef(qname);
        fn->pcheck = IncRef(pcheck);
        fn->native = func->func;
        fn->base = IncRef(base);
        fn->gns = IncRef(ns);
    }

    Release(name);
    Release(qname);
    Release(doc);
    Release(pcheck);

    return fn;
}
