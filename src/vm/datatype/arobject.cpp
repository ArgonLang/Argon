// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"
#include "error.h"
#include "function.h"
#include "namespace.h"
#include "nativewrapper.h"
#include "nil.h"

#include "arobject.h"

using namespace argon::vm::datatype;

ArObject *MROSearch(const TypeInfo *type, ArObject *key, AttributeProperty *aprop) {
    if (type->_t1 == nullptr)
        return nullptr;

    const auto *mro = (Tuple *) type->_t1;
    for (ArSize i = 0; i < mro->length; i++) {
        const auto *cursor = (const TypeInfo *) mro->objects[i];

        if (cursor->tp_map != nullptr) {
            auto *ret = NamespaceLookup((Namespace *) cursor->tp_map, key, aprop);
            if (ret != nullptr)
                return ret;
        }
    }

    return nullptr;
}

ArObject *type_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArObject *type_get_attr(const ArObject *self, ArObject *key, bool static_attr) {
    const TypeInfo *ancestor = AR_GET_TYPE(self);
    auto **ns = (Namespace **) AR_GET_NSOFFSET(self);
    const ArObject *instance = nullptr;
    ArObject *ret = nullptr;

    AttributeProperty aprop{};

    if (!AR_HAVE_OBJECT_BEHAVIOUR(self)) {
        ErrorFormat(kAttributeError[0], static_attr ? kAttributeError[2] : kAttributeError[1], ancestor->name);
        return nullptr;
    }

    if (static_attr && !AR_TYPEOF(self, type_type_)) {
        ErrorFormat(kTypeError[0], kTypeError[1], AR_TYPE_NAME(self));
        return nullptr;
    }

    // TODO if (argon::vm::GetRoutine()->frame != nullptr)
    //        instance = argon::vm::GetRoutine()->frame->instance;

    if (!static_attr && AR_SLOT_OBJECT(self)->namespace_offset >= 0)
        ret = NamespaceLookup(*ns, key, &aprop);

    if (ret == nullptr) {
        if (ancestor->tp_map != nullptr)
            ret = NamespaceLookup((Namespace *) ancestor->tp_map, key, &aprop);

        if (ret == nullptr && ancestor->_t1 != nullptr)
            ret = MROSearch(ancestor, key, &aprop);
    }

    if (ret == nullptr) {
        ErrorFormat(kAttributeError[0], kAttributeError[3], ARGON_RAW_STRING((String *) key), ancestor->name);
        return nullptr;
    }

    if (static_attr && !aprop.IsConstant()) {
        ErrorFormat(kAccessViolationError[0], kAccessViolationError[2],
                    ARGON_RAW_STRING((String *) key), ancestor->name);
        Release(&ret);
    }

    if (!aprop.IsPublic() && !TraitIsImplemented(instance, ancestor)) {
        ErrorFormat(kAccessViolationError[0], kAccessViolationError[1],
                    ARGON_RAW_STRING((String *) key), ancestor->name);
        Release(&ret);
    }

    if (AR_TYPEOF(ret, type_native_wrapper_)) {
        auto *value = NativeWrapperGet((NativeWrapper *) ret, self);

        Release(ret);

        return value;
    }

    return ret;
}

ArObject *type_repr(const TypeInfo *self) {
    return nullptr;
}

ArSize type_hash(TypeInfo *self) {
    return (ArSize) self; // returns memory pointer as ArSize
}

bool type_dtor(TypeInfo *self) {
    /*
     * NB: Destructor is only called on dynamically generated types,
     * in fact it will never be called on basic types such as atom, bytes, decimal, etc.
     */
    argon::vm::memory::Free((void *) self->name);
    argon::vm::memory::Free((void *) self->qname);
    argon::vm::memory::Free((void *) self->doc);

    Release(self->_t1);
    Release(self->tp_map);

    return true;
}

bool type_set_attr(ArObject *self, ArObject *key, ArObject *value, bool static_attr) {
    auto **ns = (Namespace **) AR_GET_NSOFFSET(self);
    const ArObject *instance = nullptr;
    ArObject *current = nullptr;

    bool is_tpm = false;

    AttributeProperty aprop{};

    if (!AR_HAVE_OBJECT_BEHAVIOUR(self)) {
        ErrorFormat(kAttributeError[0], static_attr ? kAttributeError[2] : kAttributeError[1], AR_TYPE_NAME(self));
        return false;
    }

    // TODO if (argon::vm::GetRoutine()->frame != nullptr)
    //        instance = argon::vm::GetRoutine()->frame->instance;

    if (AR_SLOT_OBJECT(self)->namespace_offset < 0) {
        ns = (Namespace **) &(AR_GET_TYPE(self)->tp_map);
        is_tpm = true;
    }

    if ((current = NamespaceLookup(*ns, key, &aprop)) == nullptr) {
        ErrorFormat(kAttributeError[0], kAttributeError[3], AR_TYPE_NAME(self));
        return false;
    }

    if (!aprop.IsPublic() && (instance == nullptr || !AR_SAME_TYPE(instance, self))) {
        ErrorFormat(kAccessViolationError[0], kAccessViolationError[1],
                    ARGON_RAW_STRING((String *) key), AR_TYPE_NAME(self));

        Release(current);
        return false;
    }

    if (AR_TYPEOF(current, type_native_wrapper_)) {
        auto ok = NativeWrapperSet((NativeWrapper *) current, self, value);

        Release(current);

        return ok;
    }

    Release(current);

    if (is_tpm || aprop.IsConstant()) {
        ErrorFormat(kUnassignableError[0], kUnassignableError[2],
                    AR_TYPE_QNAME(self), ARGON_RAW_STRING((String *) key));

        return false;
    }

    return NamespaceSet(*ns, key, value);
}

const ObjectSlots type_objslot = {
        nullptr,
        nullptr,
        nullptr,
        type_get_attr,
        type_set_attr,
        -1
};

TypeInfo TypeType = {
        AROBJ_HEAD_INIT_TYPE,
        "Type",
        nullptr,
        nullptr,
        0,
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) type_dtor,
        nullptr,
        (ArSize_UnaryOp) type_hash,
        nullptr,
        type_compare,
        (UnaryConstOp) type_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &type_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_type_ = &TypeType;

ArObject *argon::vm::datatype::Compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    static const CompareMode reverse[] = {CompareMode::EQ, CompareMode::NE, CompareMode::LE,
                                          CompareMode::LEQ, CompareMode::GR, CompareMode::GRQ};
    static const char *str_mode[] = {"==", "!=", ">", ">=", "<", "<="};
    ArObject *result = nullptr;
    auto lc = AR_GET_TYPE(self)->compare;
    auto rc = AR_GET_TYPE(other)->compare;
    bool ne = false;

    if (mode == CompareMode::NE) {
        mode = CompareMode::EQ;
        ne = true;
    }

    if (lc != nullptr)
        result = lc(self, other, mode);

    if (result == nullptr && rc != nullptr)
        result = rc(other, self, reverse[(int) mode]);

    if (result == nullptr) {
        if (mode != CompareMode::EQ) {
            ErrorFormat(kNotImplementedError[0], kNotImplementedError[2], str_mode[(int) mode],
                        AR_TYPE_NAME(self), AR_TYPE_NAME(other));
            return nullptr;
        }

        result = (ArObject *) IncRef(False);
    }

    if (ne) {
        ne = !ArBoolToBool((Boolean *) result);
        Release(result);
        result = BoolToArBool(ne);
    }

    return result;
}

ArObject *argon::vm::datatype::IteratorGet(ArObject *object, bool reversed) {
    if (AR_GET_TYPE(object)->iter == nullptr) {
        ErrorFormat(kTypeError[0], "'%s' is not iterable", AR_TYPE_NAME(object));
        return nullptr;
    }

    return AR_GET_TYPE(object)->iter(object, reversed);
}

ArObject *argon::vm::datatype::IteratorNext(ArObject *iterator) {
    if (AR_GET_TYPE(iterator)->iter == nullptr) {
        ErrorFormat(kTypeError[0], "expected an iterator not '%s'", AR_TYPE_NAME(iterator));
        return nullptr;
    }

    return AR_GET_TYPE(iterator)->iter_next(iterator);
}

ArObject *argon::vm::datatype::AttributeLoad(const ArObject *object, ArObject *key, bool static_attr) {
    AttributeGetter aload = type_type_->object->get_attr;

    if (AR_HAVE_OBJECT_BEHAVIOUR(object) && AR_SLOT_OBJECT(object)->get_attr != nullptr)
        aload = AR_SLOT_OBJECT(object)->get_attr;

    return aload(object, key, static_attr);
}

ArObject *argon::vm::datatype::Repr(const ArObject *object) {
    auto repr = AR_GET_TYPE(object)->repr;

    if (repr != nullptr)
        return repr(object);

    return (ArObject *) StringFormat("<object %s @%p>", AR_TYPE_NAME(object), object);
}

ArObject *argon::vm::datatype::Str(ArObject *object) {
    auto str = AR_GET_TYPE(object)->str;

    if (str != nullptr)
        return str(object);

    return Repr(object);
}

bool argon::vm::datatype::AttributeSet(ArObject *object, ArObject *key, ArObject *value, bool static_attr) {
    AttributeWriter awrite = type_type_->object->set_attr;

    if (AR_HAVE_OBJECT_BEHAVIOUR(object) && AR_SLOT_OBJECT(object)->set_attr != nullptr)
        awrite = AR_SLOT_OBJECT(object)->set_attr;

    return awrite(object, key, value, static_attr);
}

bool argon::vm::datatype::BufferGet(ArObject *object, ArBuffer *buffer, BufferFlags flags) {
    const auto *buf_slot = AR_GET_TYPE(object)->buffer;

    if (buf_slot == nullptr || buf_slot->get_buffer == nullptr) {
        ErrorFormat(kTypeError[0], "bytes-like object is required, not '%s'", AR_TYPE_NAME(object));
        return false;
    }

    auto ok = buf_slot->get_buffer(object, buffer, flags);

    if (ok)
        buffer->object = IncRef(object);

    return ok;
}

bool argon::vm::datatype::BufferSimpleFill(const ArObject *object, ArBuffer *buffer, BufferFlags flags,
                                           unsigned char *raw, ArSize item_size, ArSize nelem, bool writable) {
    if (buffer == nullptr) {
        ErrorFormat(kTypeError[0], "bad call to BufferSimpleFill, buffer == nullptr");
        return false;
    }

    if (ENUMBITMASK_ISTRUE(flags, BufferFlags::WRITE) && !writable) {
        ErrorFormat(kBufferError[0], kBufferError[1], AR_TYPE_NAME(object));
        return false;
    }

    buffer->buffer = raw;
    buffer->object = nullptr; // Filled by BufferGet
    buffer->geometry.item_size = item_size;
    buffer->geometry.nelem = nelem;
    buffer->length = item_size * nelem;
    buffer->flags = flags;

    return true;
}

bool argon::vm::datatype::Equal(const ArObject *self, const ArObject *other) {
    auto *cmp = Compare(self, other, CompareMode::EQ);
    bool result = ArBoolToBool((Boolean *) cmp);

    Release(cmp);
    return result;
}

bool argon::vm::datatype::Hash(ArObject *object, ArSize *out_hash) {
    auto hash = AR_GET_TYPE(object)->hash;

    if (hash != nullptr) {
        if (out_hash != nullptr)
            *out_hash = hash(object);

        return true;
    }

    return false;
}

bool argon::vm::datatype::IsNull(const ArObject *object) {
    return object == nullptr || object == (ArObject *) Nil;
}

bool argon::vm::datatype::IsTrue(const ArObject *object) {
    if (AR_GET_TYPE(object)->is_true == nullptr)
        return true;

    return AR_GET_TYPE(object)->is_true(object);
}

bool InitMembers(TypeInfo *type) {
    auto *ns = (Namespace *) type->tp_map;

    if (type->object == nullptr)
        return true;

    // Function/Method
    if (type->object->methods != nullptr) {
        for (const FunctionDef *cursor = type->object->methods; cursor->name != nullptr; cursor++) {
            auto *fn = (ArObject *) FunctionNew(cursor, type, nullptr);
            if (fn == nullptr)
                return false;

            if (!NamespaceNewSymbol(ns, cursor->name, fn, AttributeFlag::CONST | AttributeFlag::PUBLIC)) {
                Release(fn);
                return false;
            }

            Release(fn);
        }
    }

    // TODO members

    return true;
}

bool argon::vm::datatype::TypeInit(const TypeInfo *type, ArObject *auxiliary) {
    auto *unsafe_tp = (TypeInfo *) type;

    if (ENUMBITMASK_ISTRUE(type->flags, TypeInfoFlags::INITIALIZED))
        return true;

    assert(type->tp_map == nullptr);

    // TODO: traits here

    // Build tp_map
    unsafe_tp->tp_map = IncRef(auxiliary);
    if (auxiliary == nullptr) {
        unsafe_tp->tp_map = (ArObject *) NamespaceNew();
        if (unsafe_tp->tp_map == nullptr)
            return false;
    }

    // Push members
    if (!InitMembers(unsafe_tp)) {
        Release(&unsafe_tp->tp_map);
        return false;
    }

    // TODO: Override check!

    if (type->qname == nullptr) {
        unsafe_tp->qname = type->name;

        if (!type->head_.ref_count_.IsStatic()) {
            auto qn_len = strlen(type->name);

            auto *qname = (char *) vm::memory::Alloc(qn_len + 1);
            if (qname == nullptr) {
                Release(&unsafe_tp->tp_map);
                return false;
            }

            vm::memory::MemoryCopy(qname, type->name, qn_len);
            qname[qn_len] = '\0';

            unsafe_tp->qname = qname;
        }
    }

    *((TypeInfoFlags *) &type->flags) = unsafe_tp->flags | TypeInfoFlags::INITIALIZED;

    return true;
}

bool argon::vm::datatype::TraitIsImplemented(const ArObject *object, const TypeInfo *type) {
    const TypeInfo *obj_type;

    if (object == nullptr || type == nullptr)
        return false;

    obj_type = AR_GET_TYPE(object);

    if (obj_type == type)
        return true;

    if (obj_type->_t1 == nullptr)
        return false;

    const auto *mro = (Tuple *) obj_type->_t1;
    for (ArSize i = 0; i < mro->length; i++) {
        if ((const TypeInfo *) mro->objects[i] == type)
            return true;
    }

    return false;
}

void argon::vm::datatype::BufferRelease(ArBuffer *buffer) {
    if (buffer->object == nullptr)
        return;

    if (AR_GET_TYPE(buffer->object)->buffer->rel_buffer != nullptr)
        AR_GET_TYPE(buffer->object)->buffer->rel_buffer(buffer);

    Release(&buffer->object);
}

void argon::vm::datatype::Release(ArObject *object) {
    // TODO: TMP IMPL!
    if (object == nullptr)
        return;

    if (AR_GET_RC(object).DecStrong()) {
        if (AR_GET_TYPE(object)->dtor != nullptr)
            AR_GET_TYPE(object)->dtor(object);

        argon::vm::memory::Free(object);
    }
}

