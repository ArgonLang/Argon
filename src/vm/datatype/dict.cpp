// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "arstring.h"
#include "error.h"
#include "dict.h"

using namespace argon::vm::datatype;

#define CHECK_HASHABLE(key) do {                                                                        \
    if(Hash(key) == 0)                                                                                  \
        return (ArObject *) ErrorFormat(kUnhashableError[0], kUnhashableError[1], AR_TYPE_NAME(key));   \
} while(0)

const TypeInfo DictType = {
        AROBJ_HEAD_INIT_TYPE,
        "Dict",
        nullptr,
        nullptr,
        sizeof(Dict),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
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

ArObject *argon::vm::datatype::DictLookup(const Dict *dict, ArObject *key) {
    CHECK_HASHABLE(key);

    auto entry = dict->hmap.Lookup(key);
    if (entry == nullptr)
        return nullptr;

    return IncRef(entry->value);
}

ArObject *argon::vm::datatype::DictLookup(const Dict *dict, const char *key) {
    auto *skey = StringNew(key);
    if (skey == nullptr)
        return nullptr;

    auto *ret = DictLookup(dict, (ArObject *) skey);

    Release(skey);
    return ret;
}

bool argon::vm::datatype::DictInsert(Dict *dict, ArObject *key, ArObject *value) {
    HEntry<ArObject, ArObject *> *entry;

    CHECK_HASHABLE(key);

    if ((entry = dict->hmap.Lookup(key)) != nullptr) {
        Release(entry->value);
        entry->value = IncRef(value);
        // TODO: TrackIf
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

    // TODO TrackIf
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
    CHECK_HASHABLE(key);

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
    auto *dict = MakeGCObject<Dict>(type_dict_);

    if (dict != nullptr && !dict->hmap.Initialize())
        Release(dict);

    return dict;
}

void argon::vm::datatype::DictClear(Dict *dict) {
    // TODO: stub
    assert(false);
}