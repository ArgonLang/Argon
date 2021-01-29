// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>
#include <object/datatype/error.h>
#include <object/datatype/function.h>

#include "gc.h"

#include "arobject.h"

using namespace argon::object;

bool type_is_true(ArObject *obj) {
    return true;
}

bool type_equal(ArObject *left, ArObject *right) {
    return left == right;
}

ArSize type_hash(ArObject *self) {
    return (ArSize) self; // returns memory pointer as size_t
}

const TypeInfo argon::object::type_type_ = {
        TYPEINFO_STATIC_INIT,
        "datatype",
        nullptr,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        type_equal,
        type_is_true,
        type_hash,
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

ArObject *argon::object::ArObjectGCNew(const TypeInfo *type) {
    auto obj = (ArObject *) GCNew(type->size);

    if (obj != nullptr) {
        obj->ref_count = RefBits((unsigned char) RCType::GC);
        obj->type = type;
        Track(obj); // Inform the GC to track the object
    } else argon::vm::Panic(OutOfMemoryError);

    return obj;
}

ArObject *argon::object::ArObjectNew(RCType rc, const TypeInfo *type) {
    auto obj = (ArObject *) memory::Alloc(type->size);

    if (obj != nullptr) {
        obj->ref_count = RefBits((unsigned char) rc);
        obj->type = type;
    } else argon::vm::Panic(OutOfMemoryError);

    return obj;
}

ArObject *argon::object::IteratorGet(const ArObject *obj) {
    if (!IsIterable(obj))
        return ErrorFormat(&error_type_error, "'%s' is not iterable", AR_TYPE_NAME(obj));

    return AR_GET_TYPE(obj)->iter_get((ArObject *) obj);
}

ArObject *argon::object::IteratorGetReversed(const ArObject *obj) {
    if (!IsIterableReversed(obj))
        return ErrorFormat(&error_type_error, "'%s' is not reverse iterable", AR_TYPE_NAME(obj));

    return AR_GET_TYPE(obj)->iter_rget((ArObject *) obj);
}

ArObject *argon::object::IteratorNext(ArObject *iterator) {
    ArObject *ret;

    if (!IsIterator(iterator))
        return ErrorFormat(&error_type_error, "expected an iterator not '%s'", AR_TYPE_NAME(iterator));

    if ((ret = AR_ITERATOR_SLOT(iterator)->next(iterator)) == nullptr)
        ErrorFormat(&error_exhausted_iterator, "reached the end of the collection");

    return ret;
}

ArObject *argon::object::PropertyGet(const ArObject *obj, const ArObject *key, bool member) {
    PropertyInfo pinfo{};
    ArObject *ret = nullptr;
    TypeInfo *type;

    if (AR_OBJECT_SLOT(obj) != nullptr) {
        if (member) {
            if (AR_OBJECT_SLOT(obj)->get_attr != nullptr)
                ret = AR_OBJECT_SLOT(obj)->get_attr((ArObject *) obj, (ArObject *) key);
        } else {
            if (AR_OBJECT_SLOT(obj)->get_static_attr != nullptr)
                ret = AR_OBJECT_SLOT(obj)->get_static_attr((ArObject *) obj, (ArObject *) key);
        }
    }

    if (ret == nullptr && !argon::vm::IsPanicking()) {
        type = AR_TYPEOF(obj, type_type_) ? (TypeInfo *) obj : (TypeInfo *) AR_GET_TYPE(obj);

        if (type->tp_map != nullptr) {
            ret = NamespaceGetValue((Namespace *) type->tp_map, (ArObject *) key, &pinfo);
            if (ret == nullptr)
                return ErrorFormat(&error_attribute_error, "unknown attribute '%s' for type '%s'",
                                   ((String *) key)->buffer, type->name);

            if (member && pinfo.IsStatic()) {
                Release(&ret);
                ErrorFormat(&error_attribute_error,
                            "unable to access to static attribute '%s' from instance of type '%s'",
                            ((String *) key)->buffer, type->name);
            }
        } else
            ErrorFormat(&error_attribute_error, "type '%s' has no attributes", type->name);
    }

    return ret;
}

ArObject *argon::object::ToString(ArObject *obj) {
    if (AR_GET_TYPE(obj)->str != nullptr)
        return AR_GET_TYPE(obj)->str(obj);

    return ErrorFormat(&error_runtime_error, "unimplemented slot 'str' for object '%s'", AR_TYPE_NAME(obj));
}

ArSize argon::object::Hash(ArObject *obj) {
    if (IsHashable(obj))
        return AR_GET_TYPE(obj)->hash(obj);
    return 0;
}

ArSSize argon::object::Length(const ArObject *obj) {
    if (AsSequence(obj) && AR_GET_TYPE(obj)->sequence_actions->length != nullptr)
        return AR_GET_TYPE(obj)->sequence_actions->length((ArObject *) obj);
    else if (AsMap(obj) && AR_GET_TYPE(obj)->map_actions->length != nullptr)
        return AR_GET_TYPE(obj)->map_actions->length((ArObject *) obj);

    ErrorFormat(&error_type_error, "'%s' has no len", AR_TYPE_NAME(obj));
    return -1;
}

bool argon::object::BufferGet(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags) {
    if (!IsBufferable(obj)) {
        ErrorFormat(&error_type_error, "bytes-like object is required, not '%s'", obj->type->name);
        return false;
    }

    return obj->type->buffer_actions->get_buffer(obj, buffer, flags);
}

bool argon::object::BufferSimpleFill(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags, unsigned char *raw,
                                     size_t len, bool writable) {
    if (buffer == nullptr) {
        ErrorFormat(&error_buffer_error, "bad call to BufferSimpleFill, buffer == nullptr");
        return false;
    }

    if (ENUMBITMASK_ISTRUE(flags, ArBufferFlags::WRITE) && !writable) {
        ErrorFormat(&error_buffer_error, "buffer of object '%s' is not writable", obj->type->name);
        return false;
    }

    buffer->buffer = raw;
    buffer->len = len;
    IncRef(obj);
    buffer->obj = obj;
    buffer->flags = flags;

    return true;
}

bool argon::object::PropertySet(ArObject *obj, ArObject *key, ArObject *value, bool member) {
    if (member) {
        if (AR_OBJECT_SLOT(obj) == nullptr || AR_OBJECT_SLOT(obj)->set_attr == nullptr) {
            ErrorFormat(&error_attribute_error, "'%s' object is unable to use attribute(.) operator",
                        AR_TYPE_NAME(obj));
            return false;
        }

        return AR_OBJECT_SLOT(obj)->set_attr(obj, key, value);
    }

    if (AR_OBJECT_SLOT(obj) == nullptr || AR_OBJECT_SLOT(obj)->set_static_attr == nullptr) {
        ErrorFormat(&error_scope_error, "'%s' object is unable to use scope(::) operator", AR_TYPE_NAME(obj));
        return false;
    }

    return AR_OBJECT_SLOT(obj)->set_static_attr(obj, key, value);
}

bool argon::object::TypeInit(TypeInfo *info) {
    Function *fn;

    assert(info->tp_map == nullptr);

    if (info->obj_actions == nullptr || info->obj_actions->methods == nullptr)
        return true;

    // Build namespace
    if ((info->tp_map = NamespaceNew()) == nullptr)
        return false;

    // Push methods
    for (const NativeFunc *method = info->obj_actions->methods; method->name != nullptr; method++) {
        if ((fn = FunctionNew(nullptr, method)) == nullptr) {
            Release(&info->tp_map);
            return false;
        }

        if (!NamespaceNewSymbol((Namespace *) info->tp_map, fn->name, fn,
                                PropertyInfo(PropertyType::PUBLIC | PropertyType::CONST))) {
            Release(fn);
            Release(&info->tp_map);
            return false;
        }

        Release(fn);
    }

    return true;
}

bool argon::object::VariadicCheckPositional(const char *name, int nargs, int min, int max) {
    if (nargs < min) {
        ErrorFormat(&error_type_error, "%s expected %s%d argument%s, got %d", name, (min == max ? "" : "at least "),
                    min, min == 1 ? "" : "s", nargs);
        return false;
    } else if (nargs > max) {
        ErrorFormat(&error_type_error, "%s expected %s%d argument%s, got %d", name, (min == max ? "" : "at most "),
                    max, max == 1 ? "" : "s", nargs);
        return false;
    }

    return true;
}

void argon::object::BufferRelease(ArBuffer *buffer) {
    if (buffer->obj == nullptr)
        return;

    if (buffer->obj->type->buffer_actions->rel_buffer != nullptr)
        buffer->obj->type->buffer_actions->rel_buffer(buffer);

    Release(&buffer->obj);
}

void argon::object::Release(ArObject *obj) {
    if (obj == nullptr)
        return;

    if (obj->ref_count.DecStrong()) {
        if (obj->type->cleanup != nullptr)
            obj->type->cleanup(obj);

        if (obj->ref_count.IsGcObject()) {
            UnTrack(obj);
            argon::memory::Free(GCGetHead(obj));
            return;
        }

        argon::memory::Free(obj);
    }
}
