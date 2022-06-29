// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/gc.h>

#include "bool.h"
#include "error.h"
#include "namespace.h"

using namespace argon::object;

ArObject *namespace_iter_next(HMapIterator *iter) {
    ArObject *ret;
    NsEntry *entry;

    RWLockRead lock(iter->map->lock);

    if (!HMapIteratorIsValid(iter))
        return nullptr;

    entry = ((NsEntry *) iter->current);

    ret = entry->GetObject();
    HMapIteratorNext(iter);

    return ret;
}

ArObject *namespace_iter_peak(HMapIterator *iter) {
    NsEntry *entry;

    RWLockRead lock(iter->map->lock);

    if (!HMapIteratorIsValid(iter))
        return nullptr;

    entry = ((NsEntry *) iter->current);

    return entry->GetObject();
}

HMAP_ITERATOR(namespace_iterator, namespace_iter_next, namespace_iter_peak);

void SetValueToEntry(NsEntry *entry, ArObject *value, Namespace *ns) {
    entry->store_wk = false;

    if (entry->info.IsWeak() && !value->ref_count.IsStatic()) {
        entry->weak = value->ref_count.IncWeak();
        entry->store_wk = true;
        return;
    }

    entry->value = IncRef(value);
    TrackIf(ns, value);
}

bool NewEntry(Namespace *ns, ArObject *key, ArObject *value, PropertyType info) {
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        entry->Cleanup(false);
        SetValueToEntry(entry, value, ns);
        return true;
    }

    if ((entry = HMapFindOrAllocNode<NsEntry>(&ns->hmap)) == nullptr) {
        argon::vm::Panic(error_out_of_memory);
        return false;
    }

    entry->key = IncRef(key);
    entry->info = info;

    SetValueToEntry(entry, value, ns);

    if (!HMapInsert(&ns->hmap, entry)) {
        entry->Cleanup(true);
        HMapEntryToFreeNode(&ns->hmap, entry);
        return false;
    }

    return true;
}

bool CopyEntry(Namespace *ns, NsEntry *source) {
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, source->key)) != nullptr) {
        entry->Cleanup(false);
        entry->CloneValue(source);
        return true;
    }

    if ((entry = HMapFindOrAllocNode<NsEntry>(&ns->hmap)) == nullptr) {
        argon::vm::Panic(error_out_of_memory);
        return false;
    }

    entry->key = IncRef(source->key);
    entry->CloneValue(source);

    if (!HMapInsert(&ns->hmap, entry)) {
        entry->Cleanup(true);
        HMapEntryToFreeNode(&ns->hmap, entry);
        return false;
    }

    return true;
}

bool namespace_is_true(const Namespace *self) {
    return self->hmap.len > 0;
}

ArObject *namespace_compare(Namespace *self, ArObject *other, CompareMode mode) {
    auto *o = (Namespace *) other;
    const MapEntry *tmp;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other) {
        RWLockRead self_lock(self->hmap.lock);
        RWLockRead other_lock(o->hmap.lock);

        for (auto *cursor = (MapEntry *) self->hmap.iter_begin;
             cursor != nullptr; cursor = (MapEntry *) cursor->iter_next) {
            if ((tmp = (MapEntry *) HMapLookup(&o->hmap, cursor->key)) == nullptr)
                return BoolToArBool(false);

            if (!Equal(cursor->value, tmp->value))
                return BoolToArBool(false);
        }
    }

    return BoolToArBool(true);
}

ArObject *namespace_iter_get(Namespace *self) {
    RWLockRead lock(self->hmap.lock);
    return HMapIteratorNew(&type_namespace_iterator_, self, &self->hmap, false);
}

ArObject *namespace_iter_rget(Namespace *self) {
    RWLockRead lock(self->hmap.lock);
    return HMapIteratorNew(&type_namespace_iterator_, self, &self->hmap, true);
}

void namespace_trace(Namespace *self, VoidUnaryOp trace) {
    for (auto *cur = (NsEntry *) self->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if (!cur->store_wk)
            trace(cur->value);
    }
}

void namespace_cleanup(Namespace *self) {
    HMapFinalize(&self->hmap, [](HEntry *entry) {
        ((NsEntry *) entry)->Cleanup(false);
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
        nullptr,
        nullptr,
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

    if (ns != nullptr && !HMapInit(&ns->hmap))
        Release((ArObject **) &ns);

    return ns;
}

Namespace *argon::object::NamespaceNew(Namespace *ns, PropertyType ignore) {
    RWLockRead lock(ns->hmap.lock);
    Namespace *ret;

    if ((ret = NamespaceNew()) == nullptr)
        return nullptr;

    for (auto *cur = (NsEntry *) ns->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if ((int) ignore == 0 || (int) (cur->info & ignore) == 0 && !CopyEntry(ret, cur)) {
            Release(ret);
            return nullptr;
        }
    }

    return ret;
}

ArObject *argon::object::NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info) {
    RWLockRead lock(ns->hmap.lock);
    auto *entry = (NsEntry *) HMapLookup(&ns->hmap, key);

    if (entry != nullptr) {
        if (info != nullptr)
            *info = entry->info;

        return entry->GetObject();
    }

    return nullptr;
}

List *argon::object::NamespaceMkInfo(Namespace *ns, PropertyType info, bool isinstance) {
    char buffer[10] = {};
    List *keys;

    if (ns == nullptr)
        return ListNew();

    RWLockRead lock(ns->hmap.lock);

    if ((keys = ListNew(ns->hmap.len)) == nullptr)
        return nullptr;

    for (auto *cursor = (NsEntry *) ns->hmap.iter_begin; cursor != nullptr; cursor = (NsEntry *) cursor->iter_next) {
        ArObject *tmp;
        String *key;

        bool wr = false;

        if ((cursor->info & info) != info)
            continue;

        tmp = (Function *) cursor->GetObject();

        if (tmp != nullptr && AR_TYPEOF(tmp, type_function_)) {
            const auto *fn = (Function *) tmp;
            auto arity = fn->IsMethod() ? fn->arity - 1 : fn->arity;

            if (arity > 0)
                snprintf(buffer, 10, "(%d%s)", arity, fn->IsVariadic() ? ", ..." : "");
            else
                snprintf(buffer, 10, "(%s)", fn->IsVariadic() ? "..." : "");

            key = StringNewFormat("%s%s%s",
                                  isinstance && !fn->IsMethod() ? "::" : "",
                                  ((String *) cursor->key)->buffer,
                                  buffer);

            wr = true;
        } else if (tmp != nullptr && AR_TYPEOF(tmp, type_type_) && ((TypeInfo *) tmp)->flags == TypeInfoFlags::STRUCT) {
            buffer[0] = '{';
            buffer[1] = '}';
            buffer[2] = '\0';
        }

        if (!wr) {
            key = StringNewFormat("%s%s%s",
                                  isinstance && cursor->info.IsConstant() ? "::" : "",
                                  ((String *) cursor->key)->buffer,
                                  buffer);
        }

        Release(tmp);

        if (key == nullptr) {
            Release(keys);
            return nullptr;
        }

        ListAppend(keys, key);
        Release(key);

        buffer[0] = '\0';
    }

    return keys;
}

bool argon::object::NamespaceMerge(Namespace *dst, Namespace *src) {
    if (dst == src)
        return true;

    RWLockWrite dst_lock(dst->hmap.lock);
    RWLockRead src_lock(src->hmap.lock);

    for (auto *cur = (NsEntry *) src->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if (!CopyEntry(dst, cur))
            return false;
    }

    return true;
}

bool argon::object::NamespaceMergePublic(Namespace *dst, Namespace *src) {
    if (dst == src)
        return true;

    RWLockWrite dst_lock(dst->hmap.lock);
    RWLockRead src_lock(src->hmap.lock);

    for (auto *cur = (NsEntry *) src->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if ((cur->info & PropertyType::PUBLIC) == PropertyType::PUBLIC && !CopyEntry(dst, cur))
            return false;
    }

    return true;
}

bool argon::object::NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyType info) {
    if (!IsHashable(key))
        return false;

    if (value == nullptr)
        value = IncRef(NilVal);

    RWLockWrite lock(ns->hmap.lock);
    return NewEntry(ns, key, value, info);
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

    RWLockWrite lock(ns->hmap.lock);

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        entry->Cleanup(false);
        SetValueToEntry(entry, value, ns);
        return true;
    }

    return false;
}

bool argon::object::NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info) {
    RWLockRead lock(ns->hmap.lock);
    const NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        if (info != nullptr)
            *info = entry->info;

        return true;
    }

    return false;
}

int argon::object::NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count) {
    ArSize idx = 0;
    ArSize ns_len = 0;

    RWLockWrite lock(ns->hmap.lock);

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
