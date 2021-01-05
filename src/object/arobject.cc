// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>
#include <object/datatype/error.h>

#include "gc.h"

#include "arobject.h"

using namespace argon::object;

bool type_istrue(ArObject *obj) {
    return true;
}

bool type_equal(TypeInfo *left, TypeInfo *right) {
    return left == right;
}

ArSize type_hash(ArObject *self) {
    return (ArSize) self; // returns memory pointer as size_t
}

const TypeInfo argon::object::type_type_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "datatype",
        sizeof(TypeInfo),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        type_istrue,
        (BoolBinOp) type_equal,
        nullptr,
        type_hash,
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

bool argon::object::BufferGet(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags) {
    if (!IsBufferable(obj)) {
        ErrorFormat(&error_type_error, "bytes-like object is required, not '%s'", obj->type->name);
        return false;
    }

    return obj->type->buffer_actions->get_buffer(obj, buffer, flags);
}

void argon::object::BufferRelease(ArBuffer *buffer) {
    if (buffer->obj == nullptr)
        return;

    if (buffer->obj->type->buffer_actions->rel_buffer != nullptr)
        buffer->obj->type->buffer_actions->rel_buffer(buffer);

    Release(&buffer->obj);
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

ArSize argon::object::Hash(ArObject *obj) {
    if (AR_GET_TYPE(obj)->hash != nullptr)
        return AR_GET_TYPE(obj)->hash(obj);

    ErrorFormat(&error_unhashable, "unhashable type: '%s'", AR_TYPE_NAME(obj));
    return 0;
}

ArObject * argon::object::ToString(ArObject *obj) {
    if (AR_GET_TYPE(obj)->str != nullptr)
        return AR_GET_TYPE(obj)->str(obj);

    return ErrorFormat(&error_runtime_error, "unimplemented slot 'str' for object '%s'", AR_TYPE_NAME(obj));
}

bool argon::object::IsTrue(const ArObject *obj) {
    if (IsSequence(obj) && obj->type->sequence_actions->length != nullptr)
        return obj->type->sequence_actions->length((ArObject *) obj) > 0;
    else if (IsMap(obj) && obj->type->map_actions->length != nullptr)
        return obj->type->map_actions->length((ArObject *) obj) > 0;
    if (obj->type->is_true != nullptr)
        return obj->type->is_true((ArObject *) obj);
    return false;
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
