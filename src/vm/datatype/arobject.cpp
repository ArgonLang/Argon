// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"
#include "error.h"

#include "arobject.h"
#include "namespace.h"
#include "function.h"

using namespace argon::vm::datatype;

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
        /* TODO: ERROR
        if (mode != CompareMode::EQ)
            return ErrorFormat(type_not_implemented_, "operator '%s' not supported between instance of '%s' and '%s'",
                               str_mode[(int) mode], AR_TYPE_NAME(obj), AR_TYPE_NAME(other));
                               */
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

ArObject *argon::vm::datatype::Repr(const ArObject *object) {
    auto repr = AR_GET_TYPE(object)->repr;

    if (repr != nullptr)
        return repr(object);

    return (ArObject *) StringFormat("<object %s @%p>", AR_TYPE_NAME(object), object);
}

ArObject *argon::vm::datatype::Str(const ArObject *object) {
    auto str = AR_GET_TYPE(object)->str;

    if (str != nullptr)
        return str(object);

    return Repr(object);
}

ArSize argon::vm::datatype::Hash(ArObject *object) {
    auto hash = AR_GET_TYPE(object)->hash;

    if (hash != nullptr)
        return hash(object);

    return 0;
}

bool argon::vm::datatype::IsTrue(const ArObject *object) {
    if (AR_GET_TYPE(object)->is_true == nullptr)
        return true;

    return AR_GET_TYPE(object)->is_true(object);
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

bool InitMembers(TypeInfo *type) {
    auto *ns = (Namespace *) type->tp_map;

    if (type->object == nullptr)
        return true;

    // Function/Method
    if (type->object->methods != nullptr) {
        for (const FunctionDef *cursor = type->object->methods; cursor->name != nullptr; cursor++) {
            auto *fn = (ArObject *) FunctionNew(cursor, type);
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

