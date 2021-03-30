// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>
#include <object/datatype/bool.h>
#include <object/datatype/error.h>
#include <object/datatype/function.h>
#include <object/datatype/nil.h>

#include "gc.h"
#include "arobject.h"

using namespace argon::object;

bool type_is_true(ArObject *self) {
    return true;
}

ArObject *type_compare(ArObject *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArSize type_hash(ArObject *self) {
    return (ArSize) self; // returns memory pointer as size_t
}

ArObject *type_str(ArObject *self) {
    return StringNew(((TypeInfo *) self)->name);
}

const TypeInfo argon::object::type_type_ = {
        TYPEINFO_STATIC_INIT,
        "datatype",
        nullptr,
        sizeof(TypeInfo),
        nullptr,
        nullptr,
        nullptr,
        type_compare,
        type_is_true,
        type_hash,
        type_str,
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

ArObject *argon::object::InstanceGetMethod(const ArObject *instance, const ArObject *key, bool *is_meth) {
    ArObject *ret;

    if ((ret = PropertyGet(instance, key, true)) != nullptr) {
        if (is_meth != nullptr)
            *is_meth = AR_TYPEOF(ret, type_function_) && ((Function *) ret)->IsMethod();
    }

    return ret;
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
    if (!IsIterator(iterator))
        return ErrorFormat(&error_type_error, "expected an iterator not '%s'", AR_TYPE_NAME(iterator));

    return AR_ITERATOR_SLOT(iterator)->next(iterator);
}

ArObject *argon::object::PropertyGet(const ArObject *obj, const ArObject *key, bool instance) {
    const TypeInfo *type = AR_TYPEOF(obj, type_type_) ? (TypeInfo *) obj : AR_GET_TYPE(obj);
    ArObject *ret;

    if (AR_GET_TYPE(obj)->obj_actions != nullptr) {
        if (AR_OBJECT_SLOT(obj)->get_attr != nullptr)
            return AR_OBJECT_SLOT(obj)->get_attr((ArObject *) obj, (ArObject *) key);

        if (!instance && AR_OBJECT_SLOT(obj)->get_static_attr != nullptr)
            return AR_OBJECT_SLOT(obj)->get_static_attr((ArObject *) obj, (ArObject *) key);
    }

    if (!AR_IS_TYPE_INSTANCE(obj) && instance)
        return ErrorFormat(&error_type_error, "object is not an instance of type '%s'", type->name);

    if (type->tp_map == nullptr)
        return ErrorFormat(&error_attribute_error, "type '%s' has no attributes", type->name);

    if ((ret = NamespaceGetValue((Namespace *) type->tp_map, (ArObject *) key, nullptr)) == nullptr)
        return ErrorFormat(&error_attribute_error, "unknown attribute '%s' for type '%s'",
                           ((String *) key)->buffer, type->name);

    return ret;
}

ArObject *argon::object::RichCompare(const ArObject *obj, const ArObject *other, CompareMode mode) {
    static const CompareMode reverse[] = {CompareMode::EQ, CompareMode::NE, CompareMode::LE,
                                          CompareMode::LEQ, CompareMode::GE, CompareMode::GEQ};
    static const char *str_mode[] = {"==", "!=", ">", ">=", "<", "<="};

    ArObject *result = nullptr;
    bool ne = false;

    if (mode == CompareMode::NE) {
        mode = CompareMode::EQ;
        ne = true;
    }

    if (AR_GET_TYPE(obj)->compare != nullptr)
        result = AR_GET_TYPE(obj)->compare((ArObject *) obj, (ArObject *) other, mode);

    if (result == nullptr && AR_GET_TYPE(other)->compare != nullptr)
        result = AR_GET_TYPE(other)->compare((ArObject *) other, (ArObject *) obj, reverse[(int) mode]);

    if (result == nullptr) {
        if (mode == CompareMode::EQ)
            return IncRef(False);

        return ErrorFormat(&error_not_implemented, "operator '%s' not supported between instance of '%s' and '%s'",
                           str_mode[(int) mode], AR_TYPE_NAME(obj), AR_TYPE_NAME(other));
    }

    if (ne) {
        ne = !ArBoolToBool((Bool *) result);
        Release(result);
        result = BoolToArBool(ne);
    }

    return result;
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

bool argon::object::Equal(const ArObject *obj, const ArObject *other) {
    ArObject *rich = RichCompare(obj, other, CompareMode::EQ);
    bool result = ArBoolToBool((Bool *) rich);

    Release(rich);
    return result;
}

bool argon::object::IsNull(const ArObject *obj) {
    if (obj == nullptr)
        return true;

    return AR_TYPEOF(obj, type_nil_);
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
    static PropertyType meth_flags = PropertyType::PUBLIC | PropertyType::CONST;
    Function *fn;

    assert(info->tp_map == nullptr);

    if (info->obj_actions == nullptr || info->obj_actions->methods == nullptr)
        return true;

    // Build namespace
    if ((info->tp_map = NamespaceNew()) == nullptr)
        return false;

    // Push methods
    for (const NativeFunc *method = info->obj_actions->methods; method->name != nullptr; method++) {
        if ((fn = FunctionMethodNew(nullptr, info, method)) == nullptr) {
            Release(&info->tp_map);
            return false;
        }

        if (!NamespaceNewSymbol((Namespace *) info->tp_map, fn->name, fn, PropertyInfo(meth_flags))) {
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

int argon::object::TrackRecursive(ArObject *obj) {
    auto routine = vm::GetRoutine();
    List *references = routine->references;

    // search ref on list
    if (references->len > 0) {
        if (references->objects[references->len - 1] == obj)
            return 1;

        for (ArSize i = 0; i < references->len - 1; i++)
            if (references->objects[i] == obj)
                return 1;
    }

    // not found, push it!
    if (!ListAppend(references, obj))
        return -1;

    return 0;
}

void argon::object::UntrackRecursive(ArObject *obj) {
    auto routine = vm::GetRoutine();
    List *references = routine->references;

    assert(references->objects[references->len - 1] == obj);

    ListRemove(references, references->len - 1);
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
