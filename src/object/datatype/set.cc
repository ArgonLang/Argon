// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "error.h"
#include "set.h"

using namespace argon::object;

bool set_is_true(Set *self) {
    return self->set.len > 0;
}

bool set_equal(Set *self, ArObject *other) {
    auto *o = (Set *) other;

    if (self == other)
        return true;

    if (!AR_SAME_TYPE(self, other))
        return false;

    for (HEntry *cursor = self->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (HMapLookup(&o->set, cursor->key) == nullptr)
            return false;
    }

    return true;
}

ArObject *set_str(Set *self) {
    StringBuilder sb = {};
    String *tmp = nullptr;

    if (StringBuilderWrite(&sb, (unsigned char *) "{", 1, self->set.len == 0 ? 1 : 0) < 0)
        goto error;

    for (HEntry *cursor = self->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if ((tmp = (String *) ToString(cursor->key)) == nullptr)
            goto error;

        if (StringBuilderWrite(&sb, tmp, cursor->iter_next != nullptr ? 2 : 1) < 0)
            goto error;

        Release(tmp);

        if (cursor->iter_next != nullptr) {
            if (StringBuilderWrite(&sb, (unsigned char *) ", ", 2) < 0)
                goto error;
        }
    }

    if (StringBuilderWrite(&sb, (unsigned char *) "}", 1) < 0)
        goto error;

    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    return nullptr;
}

void set_cleanup(Set *self) {
    HMapFinalize(&self->set, nullptr);
}

const TypeInfo argon::object::type_set_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "set",
        sizeof(Set),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) set_is_true,
        (BoolBinOp) set_equal,
        nullptr,
        nullptr,
        (UnaryOp) set_str,
        nullptr,
        nullptr,
        (VoidUnaryOp) set_cleanup
};

Set *argon::object::SetNew() {
    auto set = ArObjectGCNew<Set>(&type_set_);

    if (set != nullptr) {
        if (!HMapInit(&set->set))
            Release((ArObject **) &set);
    }

    return set;
}

bool argon::object::SetAdd(Set *set, ArObject *value) {
    HEntry *entry;

    if (!IsHashable(value))
        return false;

    if (HMapLookup(&set->set, value) != nullptr)
        return true;

    if ((entry = HMapFindOrAllocNode<HEntry>(&set->set)) == nullptr) {
        argon::vm::Panic(OutOfMemoryError);
        return false;
    }

    IncRef(value);
    entry->key = value;

    if (!HMapInsert(&set->set, entry)) {
        Release(value);
        HMapEntryToFreeNode(&set->set, entry);
        argon::vm::Panic(OutOfMemoryError);
        return false;
    }

    return true;
}

