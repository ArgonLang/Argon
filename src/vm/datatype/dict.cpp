// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "arstring.h"
#include "boolean.h"
#include "error.h"
#include "stringbuilder.h"

#include "dict.h"

using namespace argon::vm::datatype;

#define CHECK_HASHABLE(key, retval)                                                     \
    do {                                                                                \
        if(!Hash(key, nullptr)) {                                                       \
            ErrorFormat(kUnhashableError[0], kUnhashableError[1], AR_TYPE_NAME(key));   \
            return retval;                                                              \
        }                                                                               \
    } while(0)

ArObject *dict_compare(Dict *self, ArObject *other, CompareMode mode) {
    auto *o = (Dict *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    // *** WARNING ***
    // Why std::unique_lock? See vm/sync/rsm.h
    std::unique_lock self_lock(self->rwlock);
    std::unique_lock other_lock(o->rwlock);

    if (self->hmap.length != o->hmap.length)
        return BoolToArBool(false);

    for (auto *cursor = self->hmap.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        const auto *other_entry = o->hmap.Lookup(cursor->key);

        if (other_entry == nullptr)
            return BoolToArBool(false);

        if (!Equal(cursor->value, other_entry->value))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
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

    return true;
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
        nullptr,
        (CompareOp) dict_compare,
        (UnaryConstOp) dict_repr,
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
const TypeInfo *argon::vm::datatype::type_dict_ = &DictType;

ArObject *argon::vm::datatype::DictLookup(Dict *dict, ArObject *key) {
    CHECK_HASHABLE(key, nullptr);

    std::shared_lock _(dict->rwlock);

    auto entry = dict->hmap.Lookup(key);
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
    HEntry<ArObject, ArObject *> *entry;

    CHECK_HASHABLE(key, false);

    std::unique_lock _(dict->rwlock);

    if ((entry = dict->hmap.Lookup(key)) != nullptr) {
        Release(entry->value);
        entry->value = IncRef(value);

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

bool argon::vm::datatype::DictRemove(Dict *dict, ArObject *key) {
    CHECK_HASHABLE(key, false);

    std::unique_lock _(dict->rwlock);

    auto *entry = dict->hmap.Remove(key);
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

Dict *argon::vm::datatype::DictNew() {
    auto *dict = MakeGCObject<Dict>(type_dict_, false);

    if (dict != nullptr) {
        if (!dict->hmap.Initialize()) {
            Release(dict);

            return nullptr;
        }

        new(&dict->rwlock)sync::RecursiveSharedMutex();
    }

    return dict;
}

void argon::vm::datatype::DictClear(Dict *dict) {
    std::unique_lock _(dict->rwlock);

    dict->hmap.Clear([](HEntry<ArObject, ArObject *> *entry) {
        Release(entry->key);
        Release(entry->value);
    });
}