// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "nil.h"
#include "error.h"
#include "namespace.h"
#include "bool.h"

using namespace argon::object;

void SetValueToEntry(NsEntry *entry, ArObject *value) {
    if (entry->info.IsWeak()) {
        entry->weak.DecWeak();
        entry->weak = value->ref_count.IncWeak();
    } else {
        Release(entry->value);
        IncRef(value);
        entry->value = value;
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

void namespace_cleanup(Namespace *self) {
    HMapFinalize(&self->hmap, [](HEntry *entry) {
        auto *nse = (NsEntry *) entry;

        if (nse->info.IsWeak())
            nse->weak.DecWeak();
        else
            Release(nse->value);
    });
}

void namespace_trace(Namespace *self, VoidUnaryOp trace) {
    for (auto *cur = (NsEntry *) self->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next)
        trace(cur->value);
}

const TypeInfo argon::object::type_namespace_ = {
        TYPEINFO_STATIC_INIT,
        "namespace",
        nullptr,
        sizeof(Namespace),
        nullptr,
        (VoidUnaryOp) namespace_cleanup,
        (Trace) namespace_trace,
        (CompareOp) namespace_compare,
        (BoolUnaryOp) namespace_is_true,
        nullptr,
        (UnaryOp) namespace_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Namespace *argon::object::NamespaceNew() {
    auto ns = ArObjectGCNew<Namespace>(&type_namespace_);

    if (ns != nullptr) {
        if (!HMapInit(&ns->hmap))
            Release((ArObject **) &ns);
    }

    return ns;
}

Namespace *argon::object::NamespaceNew(Namespace *ns, PropertyType ignore) {
    Namespace *space;

    if ((space = NamespaceNew()) == nullptr)
        return nullptr;

    for (auto *cur = (NsEntry *) ns->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if ((int) ignore == 0 || (int) (cur->info & ignore) == 0) {
            if (!NamespaceNewSymbol(space, cur->key, cur->value, cur->info)) {
                Release(space);
                return nullptr;
            }
        }
    }

    return space;
}

ArObject *argon::object::NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info) {
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

bool argon::object::NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyInfo info) {
    NsEntry *entry;

    if (!IsHashable(key))
        return false;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        SetValueToEntry(entry, value);
        return true;
    }

    if ((entry = HMapFindOrAllocNode<NsEntry>(&ns->hmap)) == nullptr) {
        argon::vm::Panic(OutOfMemoryError);
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

    return true;
}

bool argon::object::NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, PropertyInfo info) {
    auto *entry = (NsEntry *) HMapLookup(&ns->hmap, key);
    String *skey;

    if (entry != nullptr) {
        SetValueToEntry(entry, value);
        return true;
    }

    if ((entry = HMapFindOrAllocNode<NsEntry>(&ns->hmap)) == nullptr) {
        argon::vm::Panic(OutOfMemoryError);
        return false;
    }

    if ((skey = StringIntern(key)) == nullptr) {
        HMapEntryToFreeNode(&ns->hmap, entry);
        return false;
    }

    entry->info = info;
    entry->key = skey;

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

    return true;
}

bool argon::object::NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value) {
    NsEntry *entry;

    if (!IsHashable(key))
        return false;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        SetValueToEntry(entry, value);
        return true;
    }

    return false;
}

bool argon::object::NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info) {
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapLookup(&ns->hmap, key)) != nullptr) {
        if (info != nullptr)
            *info = entry->info;

        return true;
    }

    return false;
}

bool argon::object::NamespaceRemove(Namespace *ns, ArObject *key) {
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

    for (auto *cur = (NsEntry *) ns->hmap.iter_begin; cur != nullptr; cur = (NsEntry *) cur->iter_next) {
        if (idx >= count)
            return -1;

        ns_len++;

        if (cur->info.IsConstant())
            continue;

        SetValueToEntry(cur, values[idx++]);
    }

    return count == ns_len ? 0 : 1;
}