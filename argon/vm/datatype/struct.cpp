// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>

#include <argon/vm/memory/gc.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/struct.h>

using namespace argon::vm::datatype;

const ObjectSlots struct_objslot = {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        offsetof(Struct, ns)
};

ArObject *struct_compare(Struct *self, ArObject *other, CompareMode mode) {
    const char *m_name;

    switch (mode) {
        case CompareMode::EQ:
            m_name = "__eq";
            break;
        case CompareMode::GR:
            m_name = "__gr";
            break;
        case CompareMode::GRQ:
            m_name = "__grq";
            break;
        case CompareMode::LE:
            m_name = "__le";
            break;
        case CompareMode::LEQ:
            m_name = "__leq";
            break;
        default:
            assert(false);
    }

    ArObject *args[2];

    auto *meth = (Function *) AttributeLoadMethod((ArObject *) self, m_name);
    if (meth == nullptr) {
        argon::vm::DiscardLastPanic();

        if (mode == CompareMode::EQ)
            return BoolToArBool((ArObject *) self == other);

        return nullptr;
    }

    args[0] = (ArObject *) self;
    args[1] = other;

    auto *res = argon::vm::EvalRaiseError(meth, args, 2, argon::vm::OpCodeCallMode::FASTCALL);

    Release(meth);

    if (IsNull(res)) {
        Release(res);

        return nullptr;
    }

    if (!AR_TYPEOF(res, type_boolean_)) {
        ErrorFormat(kTypeError[0], kTypeError[12], m_name, type_boolean_->name, AR_TYPE_QNAME(res));

        Release(res);

        return nullptr;
    }

    return res;
}

bool struct_dtor(Struct *self) {
    Release(self->ns);

    return true;
}

void struct_trace(Struct *self, Void_UnaryOp trace) {
    trace((ArObject *) self->ns);
}

const TypeInfo StructType = {
        AROBJ_HEAD_INIT_TYPE,
        "Struct",
        nullptr,
        nullptr,
        sizeof(Struct),
        TypeInfoFlags::STRUCT | TypeInfoFlags::WEAKABLE,
        nullptr,
        (Bool_UnaryOp) struct_dtor,
        (TraceOp) struct_trace,
        nullptr,
        nullptr,
        (CompareOp) struct_compare,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &struct_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArObject *argon::vm::datatype::StructTypeNew(const String *name, const String *qname, const String *doc,
                                             Namespace *ns, TypeInfo **bases, unsigned int count) {
    return TypeNew(&StructType,
                   (const char *) ARGON_RAW_STRING(name),
                   (const char *) ARGON_RAW_STRING(qname),
                   (const char *) ARGON_RAW_STRING(doc),
                   (ArObject *) ns,
                   bases,
                   count);
}

Struct *argon::vm::datatype::StructNew(TypeInfo *type, ArObject **argv, unsigned int argc, OpCodeInitMode mode) {
    Namespace *ns;
    Struct *ret;

    if (!AR_TYPEOF(type, type_type_)) {
        ErrorFormat(kTypeError[0], kTypeError[1], AR_TYPE_NAME(type));
        return nullptr;
    }

    if (ENUMBITMASK_ISFALSE(type->flags, TypeInfoFlags::STRUCT)) {
        ErrorFormat(kTypeError[0], "'%s' does not represent a struct type", type->name);
        return nullptr;
    }

    if ((ns = NamespaceNew((Namespace *) type->tp_map, AttributeFlag::CONST)) == nullptr)
        return nullptr;

    if (mode == OpCodeInitMode::POSITIONAL) {
        if (!NamespaceSetPositional(ns, argv, argc)) {
            ErrorFormat(kUndeclaredeError[0], kUndeclaredeError[1], type->name);
            Release(ns);
            return nullptr;
        }
    } else {
        assert((argc & 1) == 0);

        for (ArSize i = 0; i < argc; i += 2) {
            if (!NamespaceSet(ns, argv[i], argv[i + 1])) {
                ErrorFormat(kUndeclaredeError[0], kUndeclaredeError[3], type->name,
                            ARGON_RAW_STRING((String *) argv[i]));

                Release(ns);
                return nullptr;
            }
        }
    }

    if ((ret = MakeGCObject<Struct>(type, true)) == nullptr) {
        Release(ns);
        return nullptr;
    }

    ret->ns = ns;

    return ret;
}