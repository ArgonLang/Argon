// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <shared_mutex>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/stringbuilder.h>

#include <argon/vm/datatype/set.h>

using namespace argon::vm::datatype;

// Prototypes

bool SetAddNoLock(Set *set, ArObject *object);

// ***

ARGON_FUNCTION(set_set, Set,
               "Creates an empty set or construct it from an iterable object.\n"
               "\n"
               "- Parameter iter: Iterable object.\n"
               "- Returns: New set.\n",
               nullptr, true, false) {
    if (!VariadicCheckPositional(set_set.name, (unsigned int) argc, 0, 1))
        return nullptr;

    if (argc == 1)
        return (ArObject *) SetNew(*args);

    return (ArObject *) SetNew();
}

ARGON_METHOD(set_add, add,
             "Adds an element to the set.\n"
             "\n"
             "- Parameter object: Element to add.\n"
             "- Returns: Set itself.\n",
             ": object", false, false) {
    if (!SetAdd((Set *) _self, *args))
        return nullptr;

    return IncRef(_self);
}

ARGON_METHOD(set_clear, clear,
             "Removes all the elements from the set.\n"
             "\n"
             "- Returns: Set itself.\n",
             nullptr, false, false) {
    SetClear((Set *) _self);

    return IncRef(_self);
}

ARGON_METHOD(set_contains, contains,
             "Check if this set contains the specified element.\n"
             "\n"
             "- Parameter object: Object whose presence in this set is to be tested.\n"
             "- Returns: True if the element is present, otherwise false.\n",
             ": object", false, false) {
    return BoolToArBool(SetContains((Set *) _self, *args));
}

ARGON_METHOD(set_diff, diff,
             "Removes the items in this set that are also included in another set(s).\n"
             "\n"
             "- Parameter ...sets: Another sets.\n"
             "- Returns: Set itself.\n",
             nullptr, true, false) {
    auto *self = (Set *) _self;
    SetEntry *tmp;

    for (ArSize i = 0; i < argc; i++) {
        if (!AR_SAME_TYPE(self, args[i])) {
            ErrorFormat(kTypeError[0], "%s::diff() expect type %s not '%s'", type_set_->name,
                        type_set_->name, AR_TYPE_NAME(args[i]));

            return nullptr;
        }

        if (self == (Set *) args[i]) {
            SetClear(self);

            return IncRef(_self);
        }
    }

    std::unique_lock _(self->rwlock);

    for (ArSize i = 0; i < argc; i++) {
        auto *other = (Set *) args[i];

        std::shared_lock other_lock(other->rwlock);

        for (SetEntry *cursor = self->set.iter_begin; cursor != nullptr; cursor = tmp) {
            tmp = cursor->iter_next;

            SetEntry *entry;

            other->set.Lookup(cursor->key, &entry);

            if (entry != nullptr) {
                self->set.Remove(cursor->key, &entry);

                Release(entry->key);

                self->set.FreeHEntry(entry);
            }
        }
    }

    return IncRef(_self);
}

ARGON_METHOD(set_discard, discard,
             "Remove the specified item.\n"
             "\n"
             "- Parameter ...object: Object to remove from set.\n"
             "- Returns: Set itself.\n",
             nullptr, true, true) {
    auto *self = (Set *) _self;
    SetEntry *tmp;

    for (ArSize i = 0; i < argc; i++) {
        if (!Hash(args[i], nullptr)) {
            ErrorFormat(kUnhashableError[0], kUnhashableError[1], AR_TYPE_NAME(args[i]));

            return nullptr;
        }
    }

    std::unique_lock _(self->rwlock);

    for (ArSize i = 0; i < argc; i++) {
        if (_self == args[i])
            continue;

        self->set.Remove(args[i], &tmp);

        if (tmp != nullptr) {
            Release(tmp->key);

            self->set.FreeHEntry(tmp);
        }
    }

    return IncRef(_self);
}

ARGON_METHOD(set_intersect, intersect,
             "Removes the items in this set that are not present in other, specified set(s)\n"
             "\n"
             "- Parameter ...sets: Another sets.\n"
             "- Returns: Set itself.\n",
             nullptr, true, false) {
    auto *self = (Set *) _self;
    SetEntry *tmp;

    for (ArSize i = 0; i < argc; i++) {
        if (!AR_SAME_TYPE(self, args[i])) {
            ErrorFormat(kTypeError[0], "%s::intersect() expect type %s not '%s'", type_set_->name,
                        type_set_->name, AR_TYPE_NAME(args[i]));

            return nullptr;
        }
    }

    std::unique_lock _(self->rwlock);

    for (ArSize i = 0; i < argc; i++) {
        auto *other = (Set *) args[i];

        if (self == other)
            continue;

        std::shared_lock other_lock(other->rwlock);

        for (auto *cursor = self->set.iter_begin; cursor != nullptr; cursor = tmp) {
            tmp = cursor->iter_next;

            SetEntry *entry;

            other->set.Lookup(cursor->key, &entry);

            if (entry == nullptr) {
                self->set.Remove(cursor->key, &entry);

                Release(entry->key);

                self->set.FreeHEntry(entry);
            }
        }
    }

    return IncRef(_self);
}

ARGON_METHOD(set_symdiff, symdiff,
             "Inserts the symmetric differences from this set and another.\n"
             "\n"
             "- Parameter set: Another sets.\n"
             "- Returns: Set itself.\n",
             "S: set", false, false) {
    auto *self = (Set *) _self;
    auto *other = (Set *) *args;

    SetEntry *tmp;

    if (_self == *args) {
        SetClear(self);

        return IncRef(_self);
    }

    std::unique_lock _(self->rwlock);
    std::shared_lock other_lcok(other->rwlock);

    for (auto *cursor = other->set.iter_begin; cursor != nullptr; cursor = tmp) {
        tmp = cursor->iter_next;

        SetEntry *entry;
        self->set.Remove(cursor->key, &entry);

        if (entry != nullptr) {
            Release(entry->key);

            self->set.FreeHEntry(entry);
        } else
            SetAddNoLock(self, cursor->key);
    }

    return IncRef(_self);
}

ARGON_METHOD(set_update, update,
             "Update the set with the union of this set and others.\n"
             "\n"
             "- Parameter ...sets: Another sets.\n"
             "- Returns: Set itself.\n",
             nullptr, true, false) {
    auto *self = (Set *) _self;

    for (ArSize i = 0; i < argc; i++) {
        if (!AR_SAME_TYPE(self, args[i])) {
            ErrorFormat(kTypeError[0], "%s::update() expect type %s not '%s'", type_set_->name,
                        type_set_->name, AR_TYPE_NAME(args[i]));

            return nullptr;
        }
    }

    std::unique_lock _(self->rwlock);

    for (ArSize i = 0; i < argc; i++) {
        auto *other = (Set *) args[i];

        if (self == other)
            continue;

        std::shared_lock other_lock(other->rwlock);

        for (auto *cursor = other->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
            if (!SetAddNoLock(self, cursor->key))
                return nullptr;
        }
    }

    return IncRef(_self);
}

const FunctionDef set_methods[] = {
        set_set,

        set_add,
        set_clear,
        set_contains,
        set_diff,
        set_discard,
        set_intersect,
        set_symdiff,
        set_update,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots set_objslot = {
        set_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *set_and(ArObject *left, ArObject *right) {
    if (AR_SAME_TYPE(left, right))
        return (ArObject *) SetIntersection((Set *) left, (Set *) right);

    return nullptr;
}

ArObject *set_or(ArObject *left, ArObject *right) {
    if (AR_SAME_TYPE(left, right))
        return (ArObject *) SetUnion((Set *) left, (Set *) right);

    return nullptr;
}

ArObject *set_sub(ArObject *left, ArObject *right) {
    // difference
    if (AR_SAME_TYPE(left, right))
        return (ArObject *) SetDifference((Set *) left, (Set *) right);

    return nullptr;
}

ArObject *set_xor(ArObject *left, ArObject *right) {
    if (AR_SAME_TYPE(left, right))
        return (ArObject *) SetSymmetricDifference((Set *) left, (Set *) right);

    return nullptr;
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
        nullptr,
        nullptr,
        nullptr,
};

ArObject *set_item_in(Set *self, ArObject *key) {
    SetEntry *entry;

    std::shared_lock _(self->rwlock);

    if (!self->set.Lookup(key, &entry))
        return nullptr;

    _.unlock();

    return BoolToArBool(entry != nullptr);
}

ArSize set_length(const Set *self) {
    return self->set.length;
}

const SubscriptSlots set_subscript = {
        (ArSize_UnaryOp) set_length,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BinaryOp) set_item_in
};

ArObject *set_compare(Set *self, ArObject *other, CompareMode mode) {
    auto *o = (Set *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    // *** WARNING ***
    // Why std::unique_lock? See vm/sync/rsm.h
    std::unique_lock self_lock(self->rwlock);
    std::unique_lock other_lock(o->rwlock);

    if (self->set.length != o->set.length)
        return BoolToArBool(false);

    for (auto *cursor = self->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        SetEntry *entry;

        o->set.Lookup(cursor->key, &entry);

        if (entry == nullptr)
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArObject *set_iter(Set *self, bool reverse) {
    auto *li = MakeObject<SetIterator>(type_set_iterator_);

    if (li != nullptr) {
        std::shared_lock _(self->rwlock);

        new(&li->lock)std::mutex;

        li->iterable = IncRef(self);

        li->cursor = self->set.iter_begin;
        if (li->cursor != nullptr)
            li->cursor->ref++;

        li->reverse = reverse;
    }

    return (ArObject *) li;
}

ArObject *set_repr(Set *self) {
    StringBuilder builder{};
    ArObject *ret;

    std::shared_lock _(self->rwlock);

    builder.Write((const unsigned char *) "{", 1, self->set.length == 0 ? 1 : 256);

    for (auto *cursor = self->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        auto *key = (String *) Repr(cursor->key);

        if (key == nullptr)
            return nullptr;

        if (!builder.Write(key, 2)) {
            Release(key);

            return nullptr;
        }

        if (cursor->iter_next != nullptr)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(key);
    }

    builder.Write((const unsigned char *) "}", 1, 0);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

bool set_dtor(Set *self) {
    self->set.Finalize([](SetEntry *entry) {
        Release(entry->key);
    });

    self->rwlock.~RecursiveSharedMutex();

    return true;
}

bool set_is_true(Set *self) {
    std::shared_lock _(self->rwlock);

    return self->set.length > 0;
}

TypeInfo SetType = {
        AROBJ_HEAD_INIT_TYPE,
        "Set",
        nullptr,
        nullptr,
        sizeof(Set),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) set_dtor,
        nullptr,
        nullptr,
        (Bool_UnaryOp) set_is_true,
        (CompareOp) set_compare,
        (UnaryConstOp) set_repr,
        nullptr,
        (UnaryBoolOp) set_iter,
        nullptr,
        nullptr,
        nullptr,
        &set_objslot,
        &set_subscript,
        &set_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_set_ = &SetType;

bool argon::vm::datatype::SetAdd(Set *set, ArObject *object) {
    std::unique_lock _(set->rwlock);

    return SetAddNoLock(set, object);
}

bool argon::vm::datatype::SetContains(Set *set, ArObject *object) {
    SetEntry *entry;

    std::shared_lock _(set->rwlock);

    if (!set->set.Lookup(object, &entry))
        return false;

    return entry != nullptr;
}

bool argon::vm::datatype::SetMerge(Set *set, Set *other) {
    std::unique_lock _(set->rwlock);
    std::shared_lock right_lock(other->rwlock);

    for(auto *cur = other->set.iter_begin;cur != nullptr; cur=cur->iter_next){
        if(!SetAdd(set, cur->key))
            return false;
    }

    return true;
}

bool SetAddNoLock(Set *set, ArObject *object) {
    SetEntry *entry;

    if (!set->set.Lookup(object, &entry))
        return false;

    if (entry != nullptr)
        return true;

    if ((entry = set->set.AllocHEntry()) == nullptr)
        return false;

    entry->key = IncRef(object);

    if (!set->set.Insert(entry)) {
        Release(object);

        set->set.FreeHEntry(entry);

        return false;
    }

    return true;
}

Set *argon::vm::datatype::SetDifference(Set *left, Set *right) {
    Set *ret;

    if (left == right)
        return SetNew();

    std::shared_lock left_lock(left->rwlock);
    std::shared_lock right_lock(right->rwlock);

    if ((ret = SetNew()) == nullptr)
        return nullptr;

    for (const auto *cursor = left->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        SetEntry *entry;

        right->set.Lookup(cursor->key, &entry);

        if (entry == nullptr && !SetAddNoLock(ret, cursor->key)) {
            Release(ret);

            return nullptr;
        }
    }

    return ret;
}

Set *argon::vm::datatype::SetIntersection(Set *left, Set *right) {
    Set *ret;

    if (left == right)
        return SetNew((ArObject *) left);

    std::shared_lock left_lock(left->rwlock);
    std::shared_lock right_lock(right->rwlock);

    if ((ret = SetNew()) == nullptr)
        return nullptr;

    for (const auto *cursor = left->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        SetEntry *entry;

        right->set.Lookup(cursor->key, &entry);

        if (entry != nullptr && !SetAddNoLock(ret, cursor->key)) {
            Release(ret);

            return nullptr;
        }
    }

    return ret;
}

Set *argon::vm::datatype::SetNew(ArObject *iterable) {
    ArObject *iter;
    ArObject *tmp;

    Set *ret;

    if ((ret = SetNew()) == nullptr)
        return nullptr;

    if (AR_TYPEOF(iterable, type_set_)) {
        auto *other = (Set *) iterable;

        std::shared_lock _(other->rwlock);

        for (const auto *cursor = other->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
            if (!SetAddNoLock(ret, cursor->key)) {
                Release(ret);

                return nullptr;
            }
        }

        return ret;
    }

    if ((iter = IteratorGet(iterable, false)) == nullptr) {
        Release(ret);

        return nullptr;
    }

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!SetAddNoLock(ret, tmp)) {
            Release(tmp);
            Release(iter);
            Release(ret);

            return nullptr;
        }

        Release(tmp);
    }

    Release(iter);

    return ret;
}

Set *argon::vm::datatype::SetNew() {
    auto *set = MakeObject<Set>(type_set_);

    if (set != nullptr) {
        if (!set->set.Initialize()) {
            Release(set);

            return nullptr;
        }

        new(&set->rwlock) sync::RecursiveSharedMutex();
    }

    return set;
}

Set *argon::vm::datatype::SetSymmetricDifference(Set *left, Set *right) {
    Set *ret;
    SetEntry *entry;

    if (left == right)
        return SetNew();

    std::shared_lock left_lock(left->rwlock);
    std::shared_lock right_lock(right->rwlock);

    if ((ret = SetNew()) == nullptr)
        return nullptr;

    for (const auto *cursor = left->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        right->set.Lookup(cursor->key, &entry);

        if (entry == nullptr && !SetAddNoLock(ret, cursor->key)) {
            Release(ret);

            return nullptr;
        }
    }

    for (const auto *cursor = right->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        left->set.Lookup(cursor->key, &entry);

        if (entry == nullptr && !SetAddNoLock(ret, cursor->key)) {
            Release(ret);

            return nullptr;
        }
    }

    return ret;
}

Set *argon::vm::datatype::SetUnion(Set *left, Set *right) {
    Set *ret;

    if (left == right)
        return SetNew((ArObject *) left);

    std::shared_lock left_lock(left->rwlock);
    std::shared_lock right_lock(right->rwlock);

    if ((ret = SetNew()) == nullptr)
        return nullptr;

    for (const auto *cursor = left->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (!SetAddNoLock(ret, cursor->key)) {
            Release(ret);

            return nullptr;
        }
    }

    for (const auto *cursor = right->set.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (!SetAddNoLock(ret, cursor->key)) {
            Release(ret);

            return nullptr;
        }
    }

    return ret;
}

void argon::vm::datatype::SetClear(Set *set) {
    std::unique_lock _(set->rwlock);

    set->set.Clear([](SetEntry *entry) {
        Release(entry->key);
    });
}

// SET ITERATOR

ArObject *setiterator_iter_next(SetIterator *self) {
    auto *current = self->cursor;
    ArObject *ret;

    std::unique_lock iter_lock(self->lock);

    std::shared_lock _(self->iterable->rwlock);

    if (self->cursor == nullptr || self->cursor->key == nullptr)
        return nullptr;

    ret = IncRef(self->cursor->key);

    self->cursor = self->reverse ? self->cursor->iter_prev : self->cursor->iter_next;

    self->iterable->set.FreeHEntry(current);

    if (self->cursor != nullptr)
        self->cursor->ref++;

    return ret;
}

bool setiterator_dtor(SetIterator *self) {
    if (self->cursor != nullptr)
        self->iterable->set.FreeHEntry(self->cursor);

    Release(self->iterable);

    self->lock.~mutex();

    return true;
}

bool setiterator_is_true(SetIterator *self) {
    std::unique_lock iter_lock(self->lock);
    std::shared_lock _(self->iterable->rwlock);

    return self->cursor != nullptr || self->cursor->key != nullptr;
}

TypeInfo SetIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "SetIterator",
        nullptr,
        nullptr,
        sizeof(SetIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) setiterator_dtor,
        nullptr,
        nullptr,
        (Bool_UnaryOp) setiterator_is_true,
        nullptr,
        nullptr,
        nullptr,
        CursorIteratorIter,
        (UnaryOp) setiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_set_iterator_ = &SetIteratorType;