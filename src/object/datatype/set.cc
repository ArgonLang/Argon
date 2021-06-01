// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "bool.h"
#include "error.h"
#include "set.h"

using namespace argon::object;

ArObject *set_iter_next(HMapIterator *iter) {
    ArObject *obj;

    if (iter->current == nullptr)
        return nullptr;

    if (iter->used != iter->map->len)
        return ErrorFormat(type_runtime_error_, "Set changed size during iteration");

    obj = iter->current->key;

    iter->current = iter->reversed ? iter->current->iter_prev : iter->current->iter_next;

    return IncRef(obj);
}

ArObject *set_iter_peak(HMapIterator *iter) {
    if (iter->current == nullptr)
        return nullptr;

    if (iter->used != iter->map->len)
        return ErrorFormat(type_runtime_error_, "Set changed size during iteration");

    return IncRef(iter->current->key);
}

const IteratorSlots set_iterop = {
        (BoolUnaryOp) HMapIteratorHasNext,
        (UnaryOp) set_iter_next,
        (UnaryOp) set_iter_peak,
        (VoidUnaryOp) HMapIteratorReset
};

const TypeInfo SetIteratorType = {
        TYPEINFO_STATIC_INIT,
        "set_iterator",
        nullptr,
        sizeof(HMapIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) HMapIteratorCleanup,
        (Trace) HMapIteratorTrace,
        (CompareOp) HMapIteratorCompare,
        (BoolUnaryOp) HMapIteratorHasNext,
        nullptr,
        (UnaryOp) HMapIteratorStr,
        nullptr,
        nullptr,
        nullptr,
        &set_iterop,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ArSize set_len(Set *self) {
    return self->set.len;
}

const MapSlots set_mslots{
        (SizeTUnaryOp) set_len,
        nullptr,
        nullptr
};

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

ARGON_FUNCTION5(set_, new, "Create an empty set or construct it from an iterable object."
                           ""
                           "- Parameter [iter]: iterable object."
                           "- Returns: new set.", 0, true) {
    if (!VariadicCheckPositional("set::new", count, 0, 1))
        return nullptr;

    if (count == 1)
        return SetNewFromIterable((const ArObject *) *argv);

    return SetNew();
}

ARGON_METHOD5(set_, add,
              "Adds an element to the set."
              ""
              "- Parameter obj: Element to add."
              "- Returns: set itself.", 1, false) {
    if (!SetAdd((Set *) self, argv[0]))
        return nullptr;

    return IncRef(self);
}

ARGON_METHOD5(set_, clear,
              "Removes all the elements from the set."
              ""
              "- Returns: set itself.", 0, false) {
    SetClear((Set *) self);
    return IncRef(self);
}

ARGON_METHOD5(set_, diff,
              "Removes the items in this set that are also included in another set(s)"
              ""
              "- Parameters:"
              "     ...sets: another sets."
              "- Returns: set itself.", 0, true) {
    auto *set = (Set *) self;
    HEntry *tmp;

    for (ArSize idx = 0; idx < count; idx++) {
        if (!AR_SAME_TYPE(self, argv[idx]))
            return ErrorFormat(type_type_error_, "set::diff() expect type Set not '%s'", AR_TYPE_NAME(argv[idx]));
    }

    for (ArSize idx = 0; idx < count; idx++) {
        for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
            tmp = cursor->iter_next;
            if (HMapLookup(&((Set *) argv[idx])->set, cursor->key) != nullptr) {
                Release(cursor->key);
                HMapRemove(&set->set, cursor);
            }
        }
    }

    return IncRef(self);
}

ARGON_METHOD5(set_, discard,
              "Remove the specified item."
              ""
              "- Parameter obj: object to remove from set."
              "- Returns: set itself.", 0, true) {
    auto *set = (Set *) self;
    HEntry *tmp;

    for (ArSize idx = 0; idx < count; idx++) {
        if ((tmp = HMapLookup(&set->set, argv[idx])) != nullptr) {
            Release(tmp->key);
            HMapRemove(&set->set, tmp);
            continue;
        }

        // Check for UnashableError
        if (argon::vm::IsPanicking())
            return nullptr;
    }

    return IncRef(self);
}

ARGON_METHOD5(set_, intersect,
              "Removes the items in this set that are not present in other, specified set(s)"
              ""
              "- Parameters:"
              "     ...sets: another sets."
              "- Returns: set itself.", 0, true) {
    auto *set = (Set *) self;
    HEntry *tmp;

    for (ArSize idx = 0; idx < count; idx++) {
        if (!AR_SAME_TYPE(self, argv[idx]))
            return ErrorFormat(type_type_error_, "set::intersect() expect type Set not '%s'", AR_TYPE_NAME(argv[idx]));
    }

    for (ArSize idx = 0; idx < count; idx++) {
        for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
            tmp = cursor->iter_next;
            if (HMapLookup(&((Set *) argv[idx])->set, cursor->key) == nullptr) {
                Release(cursor->key);
                HMapRemove(&set->set, cursor);
            }
        }
    }

    return IncRef(self);
}

ARGON_METHOD5(set_, symdiff,
              "Inserts the symmetric differences from this set and another."
              ""
              "- Parameters:"
              "     ...sets: another sets."
              "- Returns: set itself.", 0, true) {
    auto *set = (Set *) self;
    HEntry *tmp;

    for (ArSize idx = 0; idx < count; idx++) {
        if (!AR_SAME_TYPE(self, argv[idx]))
            return ErrorFormat(type_type_error_, "set::symdiff() expect type Set not '%s'", AR_TYPE_NAME(argv[idx]));
    }

    for (ArSize idx = 0; idx < count; idx++) {
        auto *other = (Set *) argv[idx];

        for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
            tmp = cursor->iter_next;
            if (HMapLookup(&other->set, cursor->key) != nullptr) {
                Release(cursor->key);
                HMapRemove(&set->set, cursor);
            }
        }

        for (HEntry *cursor = other->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
            if (HMapLookup(&set->set, cursor->key) == nullptr) {
                if (!SetAdd(set, cursor->key))
                    return nullptr;
            }
        }
    }

    return IncRef(self);
}

ARGON_METHOD5(set_, update,
              "Update the set with the union of this set and others."
              ""
              "- Parameters:"
              "     ...sets: another sets."
              "- Returns: set itself.", 0, true) {
    auto *set = (Set *) self;

    for (ArSize idx = 0; idx < count; idx++) {
        if (!AR_SAME_TYPE(self, argv[idx]))
            return ErrorFormat(type_type_error_, "set::update() expect type Set not '%s'", AR_TYPE_NAME(argv[idx]));
    }

    for (ArSize idx = 0; idx < count; idx++) {
        auto *other = (Set *) argv[idx];

        for (HEntry *cursor = other->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
            if (!SetAdd(set, cursor->key))
                return nullptr;
        }
    }

    return IncRef(self);
}

const NativeFunc set_methods[] = {
        set_new_,
        set_add_,
        set_clear_,
        set_diff_,
        set_discard_,
        set_intersect_,
        set_symdiff_,
        set_update_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots set_obj = {
        set_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool set_is_true(Set *self) {
    return self->set.len > 0;
}

ArObject *set_compare(Set *self, ArObject *other, CompareMode mode) {
    auto *o = (Set *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other) {
        for (HEntry *cursor = self->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
            if (HMapLookup(&o->set, cursor->key) == nullptr)
                return BoolToArBool(false);
        }
    }

    return BoolToArBool(true);
}

ArObject *set_str(Set *self) {
    StringBuilder sb = {};
    String *tmp = nullptr;
    int rec;

    if ((rec = TrackRecursive(self)) != 0)
        return rec > 0 ? StringIntern("{...}") : nullptr;

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

    UntrackRecursive(self);
    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    UntrackRecursive(self);
    return nullptr;
}

ArObject *set_iter_get(Set *self) {
    return HMapIteratorNew(&SetIteratorType, self, &self->set, false);
}

ArObject *set_iter_rget(Set *self) {
    return HMapIteratorNew(&SetIteratorType, self, &self->set, true);
}

void set_cleanup(Set *self) {
    HMapFinalize(&self->set, nullptr);
}

void set_trace(Set *self, VoidUnaryOp trace) {
    for (auto *cur = self->set.iter_begin; cur != nullptr; cur = cur->iter_next)
        trace(cur->key);
}

const TypeInfo SetType = {
        TYPEINFO_STATIC_INIT,
        "set",
        nullptr,
        sizeof(Set),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) set_cleanup,
        (Trace) set_trace,
        (CompareOp) set_compare,
        (BoolUnaryOp) set_is_true,
        nullptr,
        (UnaryOp) set_str,
        (UnaryOp) set_iter_get,
        (UnaryOp) set_iter_rget,
        nullptr,
        nullptr,
        &set_mslots,
        nullptr,
        &set_obj,
        nullptr,
        &set_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_set_ = &SetType;

Set *argon::object::SetNew() {
    auto set = ArObjectGCNew<Set>(type_set_);

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
        return (Set *) ErrorFormat(type_type_error_, "'%s' is not iterable", AR_TYPE_NAME(iterable));

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

void argon::object::SetClear(Set *set) {
    HEntry *tmp;

    for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
        tmp = cursor->iter_next;
        Release(cursor->key);
        HMapRemove(&set->set, cursor);
    }
}