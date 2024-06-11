// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <climits>
#include <shared_mutex>

#include <argon/vm/runtime.h>
#include <argon/vm/memory/gc.h>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/option.h>
#include <argon/vm/datatype/stringbuilder.h>

#include <argon/vm/datatype/dict.h>

using namespace argon::vm::datatype;

ARGON_FUNCTION(dict_dict, Dict,
               "Create an empty dict or construct it from an iterable object.\n"
               "\n"
               "- Parameter iter: Iterable object.\n"
               "- Returns: New dict.\n",
               nullptr, true, false) {
    if (!VariadicCheckPositional(dict_dict.name, (unsigned int) argc, 0, 1))
        return nullptr;

    if (argc == 1)
        return (ArObject *) DictNew(*args);

    return (ArObject *) DictNew();
}

ARGON_METHOD(dict_clear, clear,
             "Removes all the elements from the dict.\n"
             "\n"
             "- Returns: This object.\n",
             nullptr, false, false) {
    DictClear((Dict *) _self);
    return IncRef(_self);
}

ARGON_METHOD(dict_contains, contains,
             "Check if the elements is present in the dict.\n"
             "\n"
             "- Parameter key: Key to look up in the dict.\n"
             "- Returns: True if element exists, false otherwise.\n",
             ": key", false, false) {

    auto *itm = DictLookup((Dict *) _self, args[0]);

    Release(itm);

    return BoolToArBool(itm != nullptr);
}

ARGON_METHOD(dict_get, get,
             "Returns the value of the specified key.\n"
             "\n"
             "- Parameter key: Key to look up in the dict.\n"
             "- Returns: Option<?>.\n",
             ": key", false, false) {
    auto *itm = DictLookup((Dict *) _self, args[0]);

    if (itm == nullptr)
        return (ArObject *) OptionNew();

    auto *result = OptionNew(itm);

    Release(itm);

    return (ArObject *) result;
}

ARGON_METHOD(dict_items, items,
             "Returns a list containing a tuple for each key value pair.\n"
             "\n"
             "- Returns: List containing a tuple for each key value pair.\n",
             nullptr, false, false) {
    auto *self = (Dict *) _self;
    List *ret;
    Tuple *item;

    std::shared_lock _(self->rwlock);

    if ((ret = ListNew(self->hmap.length)) == nullptr)
        return nullptr;

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if ((item = TupleNew(2)) == nullptr) {
            Release(ret);
            return nullptr;
        }

        TupleInsert(item, cursor->key, 0);
        TupleInsert(item, cursor->value, 1);

        ListAppend(ret, (ArObject *) item);

        Release(item);
    }

    return (ArObject *) ret;
}

ARGON_METHOD(dict_keys, keys,
             "Returns a list containing the dict keys.\n"
             "\n"
             "- Returns: List containing the dict keys.\n",
             nullptr, false, false) {
    auto *self = (Dict *) _self;
    List *ret;

    std::shared_lock _(self->rwlock);

    if ((ret = ListNew(self->hmap.length)) == nullptr)
        return nullptr;

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next)
        ListAppend(ret, cursor->key);

    return (ArObject *) ret;
}

ARGON_METHOD(dict_pop, pop,
             "Removes the element with the specified key.\n"
             "\n"
             "- Parameter key: Key to look up in the dict.\n"
             "- Returns: Option<?>.\n",
             ": key", false, false) {
    auto *self = (Dict *) _self;
    DictEntry *item;

    std::unique_lock _(self->rwlock);

    if (!self->hmap.Remove(args[0], &item))
        return nullptr;

    if (item == nullptr)
        return (ArObject *) OptionNew();

    auto *value = item->value;

    Release(item->key);
    Release(item->value);

    self->hmap.FreeHEntry(item);

    _.unlock();

    auto *ret = OptionNew(value);

    Release(value);

    return (ArObject *) ret;
}

ARGON_METHOD(dict_values, values,
             "Returns a list of all the values in the dict.\n"
             "\n"
             "- Returns: List of all the values in the dict.\n",
             nullptr, false, false) {
    auto *self = (Dict *) _self;
    List *ret;

    std::shared_lock _(self->rwlock);

    if ((ret = ListNew(self->hmap.length)) == nullptr)
        return nullptr;

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next)
        ListAppend(ret, cursor->value);

    return (ArObject *) ret;
}

const FunctionDef dict_methods[] = {
        dict_dict,

        dict_clear,
        dict_contains,
        dict_get,
        dict_items,
        dict_keys,
        dict_pop,
        dict_values,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots dict_objslot = {
        dict_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *dict_get_item(Dict *self, ArObject *key) {
    DictEntry *entry;

    std::shared_lock _(self->rwlock);

    if (!self->hmap.Lookup(key, &entry))
        return nullptr;

    if (entry == nullptr) {
        _.unlock();

        ErrorFormat(kKeyError[0], kKeyError[1], key);

        return nullptr;
    }

    return IncRef(entry->value);
}

ArObject *dict_item_in(Dict *self, ArObject *key) {
    DictEntry *entry;

    std::shared_lock _(self->rwlock);

    if (!self->hmap.Lookup(key, &entry))
        return nullptr;

    _.unlock();

    return BoolToArBool(entry != nullptr);
}

ArSize dict_length(const Dict *self) {
    return self->hmap.length;
}

const auto dict_set_item = (bool (*)(argon::vm::datatype::Dict *, argon::vm::datatype::ArObject *,
                                     argon::vm::datatype::ArObject *)) DictInsert;

const SubscriptSlots dict_subscript = {
        (ArSize_UnaryOp) dict_length,
        (BinaryOp) dict_get_item,
        (Bool_TernaryOp) dict_set_item,
        nullptr,
        nullptr,
        (BinaryOp) dict_item_in
};

ArObject *dict_compare(Dict *self, ArObject *other, CompareMode mode) {
    auto *o = (Dict *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    std::shared_lock self_lock(self->rwlock);
    std::shared_lock other_lock(o->rwlock);

    if (self->hmap.length != o->hmap.length)
        return BoolToArBool(false);

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        DictEntry *other_entry;

        o->hmap.Lookup(cursor->key, &other_entry);

        if (other_entry == nullptr)
            return BoolToArBool(false);

        if (!Equal(cursor->value, other_entry->value))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ArObject *dict_iter(Dict *self, bool reverse) {
    auto *li = MakeGCObject<DictIterator>(type_dict_iterator_);

    if (li != nullptr) {
        std::shared_lock _(self->rwlock);

        new(&li->lock)std::mutex;

        li->iterable = IncRef(self);

        li->cursor = self->hmap.iter_begin;
        if (li->cursor != nullptr)
            li->cursor->ref++;

        li->reverse = reverse;

        argon::vm::memory::Track((ArObject*)li);
    }

    return (ArObject *) li;
}

ArObject *dict_repr(Dict *self) {
    StringBuilder builder{};
    ArObject *ret;

    int rec;
    if ((rec = RecursionTrack((ArObject *) self)) != 0)
        return rec > 0 ? (ArObject *) StringIntern("{...}") : nullptr;

    std::shared_lock _(self->rwlock);

    builder.Write((const unsigned char *) "{", 1, self->hmap.length == 0 ? 1 : 256);

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        auto *key = (String *) Repr(cursor->key);
        auto *value = (String *) Repr(cursor->value);

        if (key == nullptr || value == nullptr) {
            Release(key);
            Release(value);

            RecursionUntrack((ArObject *) self);

            return nullptr;
        }

        if (!builder.Write(key, ARGON_RAW_STRING_LENGTH(value) + (cursor->iter_next == nullptr ? 3 : 4))) {
            Release(key);
            Release(value);

            RecursionUntrack((ArObject *) self);

            return nullptr;
        }

        builder.Write((const unsigned char *) ": ", 2, 0);

        builder.Write(value, 0);

        if (cursor->iter_next != nullptr)
            builder.Write((const unsigned char *) ", ", 2, 0);

        Release(key);
        Release(value);
    }

    builder.Write((const unsigned char *) "}", 1, 0);

    RecursionUntrack((ArObject *) self);

    if ((ret = (ArObject *) builder.BuildString()) == nullptr) {
        ret = (ArObject *) builder.GetError();

        argon::vm::Panic(ret);

        Release(&ret);
    }

    return ret;
}

bool dict_dtor(Dict *self) {
    self->hmap.Finalize([](HEntry<ArObject, ArObject *> *entry) {
        Release(entry->key);
        Release(entry->value);
    });

    self->rwlock.~RecursiveSharedMutex();

    return true;
}

bool dict_is_true(Dict *self) {
    std::shared_lock _(self->rwlock);

    return self->hmap.length > 0;
}

void dict_trace(Dict *self, Void_UnaryOp trace) {
    std::shared_lock _(self->rwlock);

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        trace(cursor->key);
        trace(cursor->value);
    }
}

TypeInfo DictType = {
        AROBJ_HEAD_INIT_TYPE,
        "Dict",
        nullptr,
        nullptr,
        sizeof(Dict),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) dict_dtor,
        (TraceOp) dict_trace,
        nullptr,
        (Bool_UnaryOp) dict_is_true,
        (CompareOp) dict_compare,
        (UnaryConstOp) dict_repr,
        nullptr,
        (UnaryBoolOp) dict_iter,
        nullptr,
        nullptr,
        nullptr,
        &dict_objslot,
        &dict_subscript,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_dict_ = &DictType;

ArObject *argon::vm::datatype::DictLookup(Dict *dict, ArObject *key) {
    DictEntry *entry;

    std::shared_lock _(dict->rwlock);

    if (!dict->hmap.Lookup(key, &entry))
        return nullptr;

    if (entry == nullptr)
        return nullptr;

    return IncRef(entry->value);
}

ArObject *argon::vm::datatype::DictLookup(Dict *dict, const char *key, ArSize length) {
    auto *skey = StringNew(key, length);
    if (skey == nullptr)
        return nullptr;

    auto *ret = DictLookup(dict, (ArObject *) skey);

    Release(skey);
    return ret;
}

bool argon::vm::datatype::DictInsert(Dict *dict, ArObject *key, ArObject *value) {
    DictEntry *entry;

    std::unique_lock _(dict->rwlock);

    if (!dict->hmap.Lookup(key, &entry))
        return false;

    if (entry != nullptr) {
        Release(entry->value);
        entry->value = IncRef(value);

        _.unlock();

        memory::TrackIf((ArObject *) dict, value);

        return true;
    }

    if ((entry = dict->hmap.AllocHEntry()) == nullptr)
        return false;

    entry->key = IncRef(key);
    entry->value = IncRef(value);

    if (!dict->hmap.Insert(entry)) {
        Release(key);
        Release(value);

        dict->hmap.FreeHEntry(entry);

        return false;
    }

    _.unlock();

    memory::TrackIf((ArObject *) dict, value);

    return true;
}

bool argon::vm::datatype::DictInsert(Dict *dict, const char *key, ArObject *value) {
    auto *skey = StringNew(key);
    if (skey == nullptr)
        return false;

    auto ok = DictInsert(dict, (ArObject *) skey, value);

    Release(skey);
    return ok;
}

bool argon::vm::datatype::DictLookup(Dict *dict, const char *key, ArObject **out) {
    (*out) = nullptr;

    auto *skey = StringNew(key, strlen(key));
    if (skey == nullptr)
        return false;

    *out = DictLookup(dict, (ArObject *) skey);

    Release(skey);
    return true;
}

bool argon::vm::datatype::DictLookupIsTrue(argon::vm::datatype::Dict *dict, const char *key, bool _default) {
    ArObject *tmp;

    if (dict != nullptr) {
        tmp = DictLookup(dict, key, strlen(key));

        if (tmp != nullptr)
            _default = IsTrue(tmp);

        Release(tmp);
    }

    return _default;
}

bool argon::vm::datatype::DictRemove(Dict *dict, ArObject *key) {
    DictEntry *entry;

    std::unique_lock _(dict->rwlock);

    if (!dict->hmap.Remove(key, &entry))
        return false;

    if (entry == nullptr)
        return false;

    Release(entry->key);
    Release(entry->value);

    dict->hmap.FreeHEntry(entry);

    return true;
}

bool argon::vm::datatype::DictRemove(Dict *dict, const char *key) {
    auto *skey = StringNew(key);
    if (skey == nullptr)
        return false;

    auto ok = DictRemove(dict, (ArObject *) skey);

    Release(skey);
    return ok;
}

Dict *argon::vm::datatype::DictMerge(Dict *dict1, Dict *dict2, bool clone) {
    Dict *merge;

    if (IsNull((ArObject *) dict1)) {
        if (clone)
            return DictNew((ArObject *) dict2);

        return IncRef(dict2);
    }

    if (IsNull((ArObject *) dict2)) {
        if (clone)
            return DictNew((ArObject *) dict1);

        return IncRef(dict1);
    }

    std::shared_lock d1(dict1->rwlock);
    std::shared_lock d2(dict2->rwlock);

    auto merge_sz = (unsigned int) (((float) (dict1->hmap.length + dict2->hmap.length + 1)) / kHashMapLoadFactor);

    if ((merge = DictNew(merge_sz)) == nullptr)
        return nullptr;

    DictEntry *entry;

    for (auto *cursor = dict1->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if ((entry = merge->hmap.AllocHEntry()) == nullptr) {
            Release(merge);

            return nullptr;
        }

        entry->key = IncRef(cursor->key);
        entry->value = IncRef(cursor->value);

        if (!merge->hmap.Insert(entry)) {
            Release(cursor->key);
            Release(cursor->value);

            merge->hmap.FreeHEntry(entry);

            Release(merge);

            return nullptr;
        }
    }

    d1.unlock();

    for (auto *cursor = dict2->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        merge->hmap.Lookup(cursor->key, &entry);

        if (entry != nullptr) {
            ErrorFormat(kValueError[0], "got multiple values for key '%s'", cursor->key);

            Release(merge);

            return nullptr;
        }

        if ((entry = merge->hmap.AllocHEntry()) == nullptr) {
            Release(merge);

            return nullptr;
        }

        entry->key = IncRef(cursor->key);
        entry->value = IncRef(cursor->value);

        if (!merge->hmap.Insert(entry)) {
            Release(cursor->key);
            Release(cursor->value);

            merge->hmap.FreeHEntry(entry);

            Release(merge);

            return nullptr;
        }
    }

    return merge;
}

Dict *argon::vm::datatype::DictNew() {
    auto *dict = MakeGCObject<Dict>(type_dict_);

    if (dict != nullptr) {
        if (!dict->hmap.Initialize()) {
            memory::GCFreeRaw((ArObject *) dict);

            return nullptr;
        }

        new(&dict->rwlock)sync::RecursiveSharedMutex();
    }

    return dict;
}

Dict *DictNewFromIterable(ArObject *iterable) {
    ArObject *iter;
    ArObject *key;
    ArObject *value;
    Dict *dict;

    bool ok;

    if (!AR_ISITERABLE(iterable)) {
        ErrorFormat(kTypeError[0], kTypeError[10], AR_TYPE_NAME(iterable));
        return nullptr;
    }

    if ((dict = DictNew()) == nullptr)
        return nullptr;

    if ((iter = IteratorGet(iterable, false)) == nullptr) {
        Release(dict);
        return nullptr;
    }

    while (true) {
        if ((key = IteratorNext(iter)) == nullptr)
            break;

        if ((value = IteratorNext(iter)) == nullptr) {
            Release(key);
            Release(iter);
            Release(dict);

            ErrorFormat(kValueError[0], "dict new require an iterable object of even length");

            return nullptr;
        }

        ok = DictInsert(dict, key, value);
        Release(key);
        Release(value);

        if (!ok) {
            Release(iter);
            Release(dict);
            return nullptr;
        }
    }

    Release(iter);
    return dict;
}

Dict *argon::vm::datatype::DictNew(ArObject *object) {
    auto *o = (Dict *) object;
    Dict *ret;

    if (!AR_TYPEOF(object, type_dict_))
        return DictNewFromIterable(object);

    if ((ret = DictNew()) == nullptr)
        return nullptr;

    std::shared_lock _(o->rwlock);

    for (auto *cur = o->hmap.iter_begin; cur != nullptr; cur = cur->iter_next) {
        if (!DictInsert(ret, cur->key, cur->value)) {
            Release((ArObject **) &ret);
            break;
        }
    }

    return ret;
}

Dict *argon::vm::datatype::DictNew(unsigned int size) {
    auto *dict = MakeGCObject<Dict>(type_dict_);

    if (dict != nullptr) {
        if (!dict->hmap.Initialize(size)) {
            memory::GCFreeRaw((ArObject *) dict);

            return nullptr;
        }

        new(&dict->rwlock)sync::RecursiveSharedMutex();
    }

    return dict;
}

IntegerUnderlying argon::vm::datatype::DictLookupInt(Dict *dict, const char *key, IntegerUnderlying _default) {
    ArObject *tmp;

    if (dict != nullptr) {
        tmp = DictLookup(dict, key, strlen(key));

        if (AR_TYPEOF(tmp, type_int_))
            _default = ((Integer *) tmp)->sint;
        else if (AR_TYPEOF(tmp, type_uint_) && ((Integer *) tmp)->uint <= LONG_MAX)
            _default = (IntegerUnderlying) ((Integer *) tmp)->uint;

        Release(tmp);
    }

    return _default;
}

String *argon::vm::datatype::DictLookupString(Dict *dict, const char *key, const char *_default) {
    String *tmp;

    if (dict != nullptr) {
        tmp = (String *) DictLookup(dict, key, strlen(key));

        if (AR_TYPEOF(tmp, type_string_))
            return tmp;

        Release(tmp);
    }

    if ((tmp = StringNew(_default)) == nullptr)
        return nullptr;

    return tmp;
}

void argon::vm::datatype::DictClear(Dict *dict) {
    std::unique_lock _(dict->rwlock);

    dict->hmap.Clear([](HEntry<ArObject, ArObject *> *entry) {
        Release(entry->key);
        Release(entry->value);
    });
}

// DICT ITERATOR

ArObject *dictiterator_iter_next(DictIterator *self) {
    auto *current = self->cursor;
    Tuple *ret;

    std::unique_lock iter_lock(self->lock);

    std::shared_lock _(self->iterable->rwlock);

    if (self->cursor == nullptr || self->cursor->key == nullptr)
        return nullptr;

    if ((ret = TupleNew(2)) != nullptr) {
        TupleInsert(ret, self->cursor->key, 0);
        TupleInsert(ret, self->cursor->value, 1);

        self->cursor = self->reverse ? self->cursor->iter_prev : self->cursor->iter_next;

        self->iterable->hmap.FreeHEntry(current);

        if (self->cursor != nullptr)
            self->cursor->ref++;
    }

    return (ArObject *) ret;
}

bool dictiterator_dtor(DictIterator *self) {
    if (self->cursor != nullptr)
        self->iterable->hmap.FreeHEntry(self->cursor);

    Release(self->iterable);

    self->lock.~mutex();

    return true;
}

bool dictiterator_is_true(DictIterator *self) {
    std::unique_lock iter_lock(self->lock);
    std::shared_lock _(self->iterable->rwlock);

    return self->cursor != nullptr || self->cursor->key != nullptr;
}

void dictiterator_trace(DictIterator *self, Void_UnaryOp trace) {
    trace((ArObject *) self->iterable);
}

TypeInfo DictIteratorType = {
        AROBJ_HEAD_INIT_TYPE,
        "DictIterator",
        nullptr,
        nullptr,
        sizeof(DictIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) dictiterator_dtor,
        (TraceOp) dictiterator_trace,
        nullptr,
        (Bool_UnaryOp) dictiterator_is_true,
        nullptr,
        nullptr,
        nullptr,
        CursorIteratorIter,
        (UnaryOp) dictiterator_iter_next,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_dict_iterator_ = &DictIteratorType;