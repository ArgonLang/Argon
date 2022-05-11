// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/gc.h>

#include "bool.h"
#include "error.h"
#include "set.h"

using namespace argon::object;

ArObject *set_iter_next(HMapIterator *iter) {
    ArObject *obj;

    RWLockRead lock(iter->map->lock);

    if(!HMapIteratorIsValid(iter))
        return nullptr;

    obj = IncRef(iter->current->key);

    HMapIteratorNext(iter);

    return obj;
}

ArObject *set_iter_peak(HMapIterator *iter) {
    RWLockRead lock(iter->map->lock);

    if(!HMapIteratorIsValid(iter))
        return nullptr;

    return IncRef(iter->current->key);
}

HMAP_ITERATOR(set_iterator, set_iter_next, set_iter_peak);

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
    Set *res = nullptr;

    if (AR_SAME_TYPE(left, right)) {
        RWLockRead l_lock(l->set.lock);
        RWLockRead r_lock(r->set.lock);

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
    }

    return res;
}

ArObject *set_and(ArObject *left, ArObject *right) {
    // intersection
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res = nullptr;

    if (AR_SAME_TYPE(left, right)) {
        RWLockRead l_lock(l->set.lock);
        RWLockRead r_lock(r->set.lock);

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
    }

    return res;
}

ArObject *set_or(ArObject *left, ArObject *right) {
    // union
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res = nullptr;

    if (AR_SAME_TYPE(left, right)) {
        RWLockRead l_lock(l->set.lock);
        RWLockRead r_lock(r->set.lock);

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
    }

    return res;
}

ArObject *set_xor(ArObject *left, ArObject *right) {
    // symmetric difference
    auto *l = (Set *) left;
    auto *r = (Set *) right;
    Set *res = nullptr;

    if (AR_SAME_TYPE(left, right)) {
        RWLockRead l_lock(l->set.lock);
        RWLockRead r_lock(r->set.lock);

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

bool SetAddNoLock(Set *set, ArObject *value) {
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

    TrackIf(set, value);
    return true;
}

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

ARGON_METHOD5(set_, contains,
              "Check if this set contains the specified element."
              ""
              "- Parameter obj: object whose presence in this set is to be tested."
              "- Returns: true if the element is present, otherwise false.", 1, false) {
    return BoolToArBool(SetContains((Set *) self, argv[0]));
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

        if (self == argv[idx]) {
            SetClear(set);
            return IncRef(self);
        }
    }

    RWLockWrite self_lock(set->set.lock);

    for (ArSize idx = 0; idx < count; idx++) {
        RWLockRead other_lock(((Set *) argv[idx])->set.lock);

        for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
            tmp = cursor->iter_next;
            if (HMapLookup(&((Set *) argv[idx])->set, cursor->key) != nullptr) {
                HMapRemove(&set->set, cursor->key);
                Release(cursor->key);
                HMapEntryToFreeNode(&set->set, cursor);
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

    RWLockWrite lock(set->set.lock);

    for (ArSize idx = 0; idx < count; idx++) {
        if ((tmp = HMapRemove(&set->set, argv[idx])) != nullptr) {
            Release(tmp->key);
            HMapEntryToFreeNode(&set->set, tmp);
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

    RWLockWrite self_lock(set->set.lock);

    for (ArSize idx = 0; idx < count; idx++) {
        if (argv[idx] != self) {
            RWLockRead other_lock(((Set *) argv[idx])->set.lock);

            for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
                tmp = cursor->iter_next;
                if (HMapLookup(&((Set *) argv[idx])->set, cursor->key) == nullptr) {
                    HMapRemove(&set->set, cursor->key);
                    Release(cursor->key);
                    HMapEntryToFreeNode(&set->set, cursor);
                }
            }
        }
    }

    return IncRef(self);
}

ARGON_METHOD5(set_, symdiff,
              "Inserts the symmetric differences from this set and another."
              ""
              "- Parameter set: another sets."
              "- Returns: set itself.", 1, false) {
    auto *set = (Set *) self;
    auto *other = (Set *) argv[0];
    HEntry *tmp;

    if (!AR_SAME_TYPE(self, other))
        return ErrorFormat(type_type_error_, "set::symdiff() expect type Set not '%s'", AR_TYPE_NAME(argv[0]));

    if (self == other) {
        SetClear(set);
        return IncRef(self);
    }

    RWLockWrite self_lock(set->set.lock);
    RWLockRead other_lock(other->set.lock);

    for (HEntry *cursor = set->set.iter_begin; cursor != nullptr; cursor = tmp) {
        tmp = cursor->iter_next;
        if (HMapLookup(&other->set, cursor->key) != nullptr) {
            HMapRemove(&set->set, cursor->key);
            Release(cursor->key);
            HMapEntryToFreeNode(&set->set, cursor);
        }
    }

    for (HEntry *cursor = other->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (HMapLookup(&set->set, cursor->key) == nullptr) {
            if (!SetAddNoLock(set, cursor->key))
                return nullptr;
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

    RWLockWrite self_lock(set->set.lock);

    for (ArSize idx = 0; idx < count; idx++) {
        if (argv[idx] != self) {
            RWLockRead other_lock(((Set *) argv[idx])->set.lock);

            for (HEntry *cursor = ((Set *) argv[idx])->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
                if (!SetAddNoLock(set, cursor->key))
                    return nullptr;
            }
        }
    }

    return IncRef(self);
}

const NativeFunc set_methods[] = {
        set_new_,
        set_add_,
        set_clear_,
        set_contains_,
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
        nullptr,
        nullptr,
        -1
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

    RWLockRead lock(self->set.lock);

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
    RWLockRead lock(self->set.lock);
    return HMapIteratorNew(&type_set_iterator_, self, &self->set, false);
}

ArObject *set_iter_rget(Set *self) {
    RWLockRead lock(self->set.lock);
    return HMapIteratorNew(&type_set_iterator_, self, &self->set, true);
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

    if (set != nullptr)
        if (!HMapInit(&set->set))
            Release((ArObject **) &set);

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
    RWLockWrite lock(set->set.lock);
    return SetAddNoLock(set, value);
}

bool argon::object::SetContains(Set *set, ArObject *value) {
    RWLockRead lock(set->set.lock);
    return HMapLookup(&set->set, value) != nullptr;
}

void argon::object::SetClear(Set *set) {
    RWLockWrite lock(set->set.lock);

    HMapClear(&set->set, nullptr);
}