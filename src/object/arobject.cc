// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arobject.h"
#include "objmgmt.h"

using namespace argon::object;

bool dtype_istrue(ArObject *obj) {
    return true;
}

bool dtype_equal(TypeInfo *left, TypeInfo *right) {
    if (right->type != &type_dtype_)
        return false;

    return left == right;
}

size_t dtype_hash(ArObject *self) {
    return (size_t) self; // returns memory pointer as size_t
}

const TypeInfo argon::object::type_dtype_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "datatype",
        sizeof(TypeInfo),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        dtype_istrue,
        (BoolBinOp)dtype_equal,
        nullptr,
        dtype_hash,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

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
