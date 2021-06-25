// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "nativewrap.h"
#include "error.h"
#include "tuple.h"
#include "struct.h"

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
        nullptr,
        nullptr,
        nullptr,
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
            goto error;

        if (!AR_TYPEOF(tmp, type_native_wrapper_))
            goto error;

        if (!NativeWrapperSet((NativeWrapper *) tmp, instance, values[i + 1])) {
            Release(tmp);
            return false;
        }
    }

    return true;

    error:
    ErrorFormat(type_undeclared_error_, "native struct '%s' have no property named '%s'",
                type->name, ((String *) values[i])->buffer);
    Release(tmp);
    return false;

}

ArObject *argon::object::StructInit(const TypeInfo *type, ArObject **values, ArSize count, bool keypair) {
    auto *instance = ArObjectGCNew(type);
    Namespace **ns;
    bool ok;

    if (instance == nullptr)
        return nullptr;

    if (type->obj_actions->nsoffset < 0) {
        if (type->tp_map != nullptr && count > 0) {
            ok = keypair ? NativeInitKeyPair(instance, values, count) : NativeInitPositional(instance, values, count);

            if (!ok) {
                Release(instance);
                return nullptr;
            }
        }

        return instance;
    }

    // Initialize new namespace
    ns = (Namespace **) AR_GET_NSOFF(instance);

    if ((*ns = NamespaceNew((Namespace *) type->tp_map, PropertyType::CONST)) == nullptr) {
        Release(instance);
        return nullptr;
    }

    if (!keypair) {
        if (NamespaceSetPositional(*ns, values, count) >= 1) {
            Release(instance);
            return (Struct *) ErrorFormat(type_undeclared_error_, "too many args to initialize struct '%s'",
                                          type->name);
        }
    } else {
        for (ArSize i = 0; i < count; i += 2) {
            if (!NamespaceSetValue(*ns, values[i], values[i + 1])) {
                Release(instance);
                return (Struct *) ErrorFormat(type_undeclared_error_, "struct '%s' have no property named '%s'",
                                              type->name, ((String *) values[i])->buffer);
            }
        }
    }

    return instance;
}