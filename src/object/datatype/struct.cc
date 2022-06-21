// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "atom.h"
#include "error.h"
#include "nativewrap.h"
#include "struct.h"
#include "tuple.h"

using namespace argon::object;
using namespace argon::memory;

const ObjectSlots struct_actions{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        offsetof(Struct, names)
};

ArObject *struct_compare(Struct *self, ArObject *other, CompareMode mode) {
    ArObject *args[] = {self, other, nullptr};
    ArObject *func;
    ArObject *ret;
    Atom *cmp_mode;
    String *key;

    if ((key = StringIntern("__cmp")) == nullptr)
        return nullptr;

    func = PropertyGet(self, key, true);
    Release(key);

    if (func != nullptr) {
        switch (mode) {
            case CompareMode::EQ:
                cmp_mode = AtomNew("EQ");
                break;
            case CompareMode::NE:
                assert(false);
            case CompareMode::GR:
                cmp_mode = AtomNew("GR");
                break;
            case CompareMode::GRQ:
                cmp_mode = AtomNew("GRQ");
                break;
            case CompareMode::LE:
                cmp_mode = AtomNew("LE");
                break;
            case CompareMode::LEQ:
                cmp_mode = AtomNew("LEQ");
                break;
        }

        if (cmp_mode == nullptr) {
            Release(func);
            return nullptr;
        }

        args[2] = cmp_mode;

        ret = argon::vm::Call(func, 3, args);

        Release(cmp_mode);
        Release(func);

        if (IsNull(ret)) {
            Release(ret);
            return nullptr;
        }

        return ret;
    }

    Release(argon::vm::GetLastError()); // Ignore undeclared_method or private_modifier
    return nullptr;
}

ArObject *struct_repr(Struct *self) {
    ArObject *args[] = {self};
    ArObject *func;
    ArObject *ret;
    String *key;

    if ((key = StringIntern("__repr")) == nullptr)
        return nullptr;

    func = PropertyGet(self, key, true);
    Release(key);

    if (func != nullptr) {
        ret = argon::vm::Call(func, 1, args);
        Release(func);
        return ret;
    }

    Release(argon::vm::GetLastError()); // Ignore undeclared_method or private_modifier
    return AR_GET_TYPE(self)->str(self);
}

ArObject *struct_str(Struct *self) {
    ArObject *args[] = {self};
    ArObject *func;
    ArObject *ret;
    String *key;

    if ((key = StringIntern("__str")) == nullptr)
        return nullptr;

    func = PropertyGet(self, key, true);
    Release(key);

    if (func != nullptr) {
        ret = argon::vm::Call(func, 1, args);
        Release(func);
        return ret;
    }

    Release(argon::vm::GetLastError()); // Ignore undeclared_method or private_modifier
    return StringNewFormat("%s object at %p", AR_TYPE_NAME(self), self);
}

void struct_cleanup(Struct *self) {
    Release(self->names);
}

void struct_trace(Struct *self, VoidUnaryOp trace) {
    trace(self->names);
}

const TypeInfo StructType = {
        TYPEINFO_STATIC_INIT,
        "struct",
        nullptr,
        sizeof(Struct),
        TypeInfoFlags::STRUCT,
        nullptr,
        (VoidUnaryOp) struct_cleanup,
        (Trace) struct_trace,
        (CompareOp) struct_compare,
        TypeInfo_IsTrue_True,
        nullptr,
        (UnaryOp) struct_repr,
        (UnaryOp) struct_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &struct_actions,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_struct_ = &StructType;

bool NativeInitPositional(ArObject *instance, ArObject **values, ArSize count) {
    const TypeInfo *type = AR_GET_TYPE(instance);
    ArObject *tmp;
    ArObject *iter;

    ArSize index = 0;
    ArSize setted = 0;

    if (type->tp_map == nullptr || count == 0)
        return true;

    if ((iter = IteratorGet(type->tp_map)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (AR_TYPEOF(tmp, type_native_wrapper_)) {
            if (!NativeWrapperSet((NativeWrapper *) tmp, instance, values[index++])) {
                Release(iter);
                Release(tmp);
                return false;
            }
            setted++;
        }
        Release(tmp);
    }

    Release(iter);

    if (count > setted) {
        ErrorFormat(type_undeclared_error_, "too many args to initialize native struct '%s'", type->name);
        return false;
    }

    return true;
}

bool NativeInitKeyPair(ArObject *instance, ArObject **values, ArSize count) {
    const TypeInfo *type = AR_GET_TYPE(instance);
    ArObject *tmp;
    ArSize i;

    if (type->tp_map == nullptr || count == 0)
        return true;

    for (i = 0; i < count; i += 2) {
        if ((tmp = NamespaceGetValue((Namespace *) type->tp_map, values[i], nullptr)) == nullptr)
            goto ERROR;

        if (!AR_TYPEOF(tmp, type_native_wrapper_))
            goto ERROR;

        if (!NativeWrapperSet((NativeWrapper *) tmp, instance, values[i + 1])) {
            Release(tmp);
            return false;
        }
    }

    return true;

    ERROR:
    ErrorFormat(type_undeclared_error_, "native struct '%s' have no property named '%s'",
                type->name, ((String *) values[i])->buffer);
    Release(tmp);
    return false;

}

ArObject *argon::object::StructInit(const TypeInfo *type, ArObject **values, ArSize count, bool keypair) {
    ArObject *instance;
    Namespace **ns;

    if (!AR_TYPEOF(type, type_type_))
        return ErrorFormat(type_type_error_, "expected datatype, not instance of '%s'", type->type->name);

    if (type->flags != TypeInfoFlags::STRUCT)
        return ErrorFormat(type_type_error_, "'%s' is not a struct datatype", type->name);

    if ((instance = ArObjectGCNewTrack(type)) == nullptr)
        return nullptr;

    if (type->obj_actions->nsoffset < 0) {
        if (type->tp_map != nullptr && count > 0) {
            if (!(keypair ? NativeInitKeyPair(instance, values, count) : NativeInitPositional(instance, values, count)))
                goto ERROR;
        }

        return instance;
    }

    // Initialize new namespace
    ns = (Namespace **) AR_GET_NSOFF(instance);

    if ((*ns = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST)) == nullptr)
        goto ERROR;

    if (!keypair) {
        if (NamespaceSetPositional(*ns, values, count) >= 1) {
            ErrorFormat(type_undeclared_error_, "too many args to initialize struct '%s'", type->name);
            goto ERROR;
        }
    } else {
        for (ArSize i = 0; i < count; i += 2) {
            if (!NamespaceSetValue(*ns, values[i], values[i + 1])) {
                ErrorFormat(type_undeclared_error_, "struct '%s' have no property named '%s'", type->name,
                            ((String *) values[i])->buffer);
                goto ERROR;
            }
        }
    }

    return instance;

    ERROR:
    Release(instance);
    return nullptr;
}