// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "error.h"
#include "set.h"

using namespace argon::object;

ArObject *set_sub(ArObject *left, ArObject *right) {
    // difference
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res;

    if (!AR_SAME_TYPE(left, right))
        return nullptr;

    if ((res = SetNew()) == nullptr)
        return nullptr;

    for (HEntry *cursor = l->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (HMapLookup(&r->set, cursor->key) == nullptr) {
            if (!SetAdd(res, cursor->key)) {
                Release(res);
                return nullptr;
            }
        }
    }

    return res;
}

ArObject *set_and(ArObject *left, ArObject *right) {
    // intersection
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res;

    if (!AR_SAME_TYPE(left, right))
        return nullptr;

    if ((res = SetNew()) == nullptr)
        return nullptr;

    for (HEntry *cursor = l->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (HMapLookup(&r->set, cursor->key) != nullptr) {
            if (!SetAdd(res, cursor->key)) {
                Release(res);
                return nullptr;
            }
        }
    }

    return res;
}

ArObject *set_or(ArObject *left, ArObject *right) {
    // union
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res;

    if (!AR_SAME_TYPE(left, right))
        return nullptr;

    if ((res = SetNew()) == nullptr)
        return nullptr;

    for (HEntry *cursor = l->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (!SetAdd(res, cursor->key)) {
            Release(res);
            return nullptr;
        }
    }

    for (HEntry *cursor = r->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (!SetAdd(res, cursor->key)) {
            Release(res);
            return nullptr;
        }
    }

    return res;
}

ArObject *set_xor(ArObject *left, ArObject *right) {
    // symmetric difference
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res;

    if (!AR_SAME_TYPE(left, right))
        return nullptr;

    if ((res = SetNew()) == nullptr)
        return nullptr;

    for (HEntry *cursor = l->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (HMapLookup(&r->set, cursor->key) == nullptr) {
            if (!SetAdd(res, cursor->key)) {
                Release(res);
                return nullptr;
            }
        }
    }

    for (HEntry *cursor = r->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (HMapLookup(&l->set, cursor->key) == nullptr) {
            if (!SetAdd(res, cursor->key)) {
                Release(res);
                return nullptr;
            }
        }
    }

    return res;
}

const OpSlots set_ops = {
        nullptr,
        set_sub,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        set_and,
        set_or,
        set_xor,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        set_sub,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

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

ArObject *set_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("set", count, 0, 1))
        return nullptr;

    if (count == 1)
        return SetNewFromIterable((const ArObject *) *args);

    return SetNew();
}

void set_cleanup(Set *self) {
    HMapFinalize(&self->set, nullptr);
}

const TypeInfo argon::object::type_set_ = {
        TYPEINFO_STATIC_INIT,
        "set",
        nullptr,
        sizeof(Set),
        set_ctor,
        (VoidUnaryOp) set_cleanup,
        nullptr,
        nullptr,
        (BoolBinOp) set_equal,
        (BoolUnaryOp) set_is_true,
        nullptr,
        (UnaryOp) set_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &set_ops
};

Set *argon::object::SetNew() {
    auto set = ArObjectGCNew<Set>(&type_set_);

    if (set != nullptr) {
        if (!HMapInit(&set->set))
            Release((ArObject **) &set);
    }

    return set;
}

Set *argon::object::SetNewFromIterable(const ArObject *iterable) {
    ArObject *iter;
    ArObject *ret;
    Set *set;

    if (!IsIterable(iterable))
        return (Set*)ErrorFormat(&error_type_error, "'%s' is not iterable", AR_TYPE_NAME(iterable));

    if ((set = SetNew()) == nullptr)
        return nullptr;

    if ((iter = IteratorGet(iterable)) == nullptr) {
        Release(set);
        return nullptr;
    }

    while ((ret = IteratorNext(iter)) != nullptr) {
        if (!SetAdd(set, ret)) {
            Release(iter);
            Release(set);
            Release(ret);
            return nullptr;
        }
        Release(ret);
    }

    Release(iter);
    return set;
}

bool argon::object::SetAdd(Set *set, ArObject *value) {
    HEntry *entry;

    if (HMapLookup(&set->set, value) != nullptr)
        return true;

    // Check for UnashableError
    if (argon::vm::IsPanicking())
        return false;

    if ((entry = HMapFindOrAllocNode<HEntry>(&set->set)) == nullptr)
        return false;

    IncRef(value);
    entry->key = value;

    if (!HMapInsert(&set->set, entry)) {
        Release(value);
        HMapEntryToFreeNode(&set->set, entry);
        return false;
    }

    return true;
}
