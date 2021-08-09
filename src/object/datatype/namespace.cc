// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/gc.h>

#include "nil.h"
#include "error.h"
#include "namespace.h"
#include "bool.h"

using namespace argon::object;

ArObject *namespace_iter_next(HMapIterator *iter) {
    ArObject *ret;
    NsEntry *entry;

    if(!HMapIteratorIsValid(iter))
        return nullptr;

    entry = ((NsEntry *) iter->current);

    ret = entry->info.IsWeak() ? ReturnNil(entry->weak.GetObject()) : IncRef(entry->value);
    HMapIteratorNext(iter);

    return ret;
}

ArObject *namespace_iter_peak(HMapIterator *iter) {
    ArObject *ret;
    NsEntry *entry;

    if(!HMapIteratorIsValid(iter))
        return nullptr;

    entry = ((NsEntry *) iter->current);
    ret = entry->info.IsWeak() ? ReturnNil(entry->weak.GetObject()) : IncRef(entry->value);

    return ret;
}

const IteratorSlots namespace_iterop = {
        (BoolUnaryOp) HMapIteratorHasNext,
        (UnaryOp) namespace_iter_next,
        (UnaryOp) namespace_iter_peak,
        (VoidUnaryOp) HMapIteratorReset
};

const TypeInfo NamespaceIteratorType = {
        TYPEINFO_STATIC_INIT,
        "namespace_iterator",
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
        &namespace_iterop,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

void SetValueToEntry(NsEntry *entry, ArObject *value, Namespace *ns) {
    if (entry->info.IsWeak()) {
        entry->weak.DecWeak();
        entry->weak = value->ref_count.IncWeak();
    } else {
        Release(entry->value);
        IncRef(value);
        entry->value = value;
        TrackIf(ns, value);
    }
}

void ReleaseEntryValue(NsEntry *entry, bool release_key) {
    if (release_key)
        Release(entry->key);

    if (entry->info.IsWeak())
        entry->weak.DecWeak();
    else
        Release(entry->value);
}

bool AddNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyType info) {
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        SetValueToEntry(entry, value, ns);
        return true;
    }

    if ((entry = HMapFindOrAllocNode<NsEntry>(&ns->hmap)) == nullptr) {
        argon::vm::Panic(error_out_of_memory);
        return false;
    }

    entry->info = info;
    IncRef(key);
    entry->key = key;

    if (!entry->info.IsWeak()) {
        IncRef(value);
        entry->value = value;
    } else
        entry->weak = value->ref_count.IncWeak();

    if (!HMapInsert(&ns->hmap, entry)) {
        ReleaseEntryValue(entry, true);
        HMapEntryToFreeNode(&ns->hmap, entry);
        return false;
    }

    if (!entry->info.IsWeak())
        TrackIf(ns, value);

    return true;
}

bool namespace_is_true(Namespace *self) {
    return self->hmap.len > 0;
}

ArObject *namespace_compare(Namespace *self, ArObject *other, CompareMode mode) {
    auto *o = (Namespace *) other;
    MapEntry *cursor;
    MapEntry *tmp;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other) {
        RWLockRead self_lock(self->lock);
        RWLockRead other_lock(o->lock);

        for (cursor = (MapEntry *) self->hmap.iter_begin; cursor != nullptr; cursor = (MapEntry *) cursor->iter_next) {
            if ((tmp = (MapEntry *) HMapLookup(&o->hmap, cursor->key)) == nullptr)
                return BoolToArBool(false);

            if (!Equal(cursor->value, tmp->value))
                return BoolToArBool(false);
        }
    }

    return BoolToArBool(true);
}

ArObject *namespace_str(Namespace *str) {
    return nullptr;
}

ArObject *namespace_iter_get(Map *self) {
    return HMapIteratorNew(&NamespaceIteratorType, self, &self->hmap, false);
}

ArObject *namespace_iter_rget(Map *self) {
    return HMapIteratorNew(&NamespaceIteratorType, self, &self->hmap, true);
}

void namespace_trace(Namespace *self, VoidUnaryOp trace) {
    for (auto *cur = (NsEntry *) self->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next)
        trace(cur->value);
}

void namespace_cleanup(Namespace *self) {
    HMapFinalize(&self->hmap, [](HEntry *entry) {
        auto *nse = (NsEntry *) entry;

        if (nse->info.IsWeak())
            nse->weak.DecWeak();
        else
            Release(nse->value);
    });
}

const TypeInfo NamespaceType = {
        TYPEINFO_STATIC_INIT,
        "namespace",
        nullptr,
        sizeof(Namespace),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) namespace_cleanup,
        (Trace) namespace_trace,
        (CompareOp) namespace_compare,
        (BoolUnaryOp) namespace_is_true,
        nullptr,
        (UnaryOp) namespace_str,
        (UnaryOp) namespace_iter_get,
        (UnaryOp) namespace_iter_rget,
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
const TypeInfo *argon::object::type_namespace_ = &NamespaceType;

Namespace *argon::object::NamespaceNew() {
    auto ns = ArObjectGCNew<Namespace>(type_namespace_);

    if (ns != nullptr) {
        ns->lock = 0;

        if (!HMapInit(&ns->hmap))
            Release((ArObject **) &ns);
    }

    return ns;
}

Namespace *argon::object::NamespaceNew(Namespace *ns, PropertyType ignore) {
    RWLockRead lock(ns->lock);
    Namespace *ret;

    if ((ret = NamespaceNew()) == nullptr)
        return nullptr;

    for (auto *cur = (NsEntry *) ns->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if ((int) ignore == 0 || (int) (cur->info & ignore) == 0) {
            if (!NamespaceNewSymbol(ret, cur->key, cur->value, (PropertyType) cur->info)) {
                Release(ret);
                return nullptr;
            }
        }
    }

    return ret;
}

ArObject *argon::object::NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info) {
    RWLockRead lock(ns->lock);
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        if (info != nullptr)
            *info = entry->info;

        if (entry->info.IsWeak())
            return ReturnNil(entry->weak.GetObject());

        IncRef(entry->value);
        return entry->value;
    }

    return nullptr;
}

bool argon::object::NamespaceMerge(Namespace *dst, Namespace *src) {
    if (dst == src)
        return true;

    RWLockWrite dst_lock(dst->lock);
    RWLockRead src_lock(src->lock);

    for (auto *cur = (NsEntry *) src->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if (!AddNewSymbol(dst, cur->key, cur->value, (PropertyType) cur->info))
            return false;
    }

    return true;
}

bool argon::object::NamespaceMergePublic(Namespace *dst, Namespace *src) {
    if (dst == src)
        return true;

    RWLockWrite dst_lock(dst->lock);
    RWLockRead src_lock(src->lock);

    for (auto *cur = (NsEntry *) src->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if ((cur->info & PropertyType::PUBLIC) == PropertyType::PUBLIC) {
            if (!AddNewSymbol(dst, cur->key, cur->value, (PropertyType) cur->info))
                return false;
        }
    }

    return true;
}

bool argon::object::NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyType info) {
    if (!IsHashable(key))
        return false;

    if (value == nullptr)
        value = IncRef(NilVal);

    RWLockWrite lock(ns->lock);
    return AddNewSymbol(ns, key, value, info);
}

bool argon::object::NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, PropertyType info) {
    String *skey;
    bool ok;

    if ((skey = StringIntern(key)) == nullptr)
        return false;

    ok = NamespaceNewSymbol(ns, skey, value, info);
    Release(skey);

    return ok;
}

bool argon::object::NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value) {
    NsEntry *entry;

    if (!IsHashable(key))
        return false;

    RWLockWrite lock(ns->lock);

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        SetValueToEntry(entry, value, ns);
        return true;
    }

    return false;
}

bool argon::object::NamespaceSetValue(Namespace *ns, const char *key, ArObject *value) {
    RWLockWrite lock(ns->lock);
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        SetValueToEntry(entry, value, ns);
        return true;
    }

    return false;
}

bool argon::object::NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info) {
    RWLockRead lock(ns->lock);
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        if (info != nullptr)
            *info = entry->info;

        return true;
    }

    return false;
}

bool argon::object::NamespaceRemove(Namespace *ns, ArObject *key) {
    RWLockWrite lock(ns->lock);
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapRemove(&ns->hmap, key)) != nullptr) {
        ReleaseEntryValue(entry, true);
        HMapEntryToFreeNode(&ns->hmap, entry);
        return true;
    }

    return false;
}

int argon::object::NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count) {
    ArSize idx = 0;
    ArSize ns_len = 0;

    RWLockWrite lock(ns->lock);

    for (auto *cur = (NsEntry *) ns->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if (idx >= count)
            return -1;

        ns_len++;

        if (cur->info.IsConstant())
            continue;

        SetValueToEntry(cur, values[idx++], ns);
    }

    return count == ns_len ? 0 : 1;
}
