// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "nil.h"
#include "error.h"
#include "namespace.h"

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

bool namespace_equal(Namespace *self, ArObject *other) {
    auto *o = (Namespace *) other;
    NsEntry *tmp;

    if (self == other)
        return true;

    if (!AR_SAME_TYPE(self, other))
        return false;

    for (auto *cursor = (NsEntry *) self->hmap.iter_begin; cursor != nullptr; cursor = (NsEntry *) cursor->iter_next) {
        if ((tmp = (NsEntry *) HMapLookup(&o->hmap, cursor->key)) == nullptr || !AR_EQUAL(cursor->value, tmp->value))
            return false;
    }

    return true;
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
        (const unsigned char *) "namespace",
        sizeof(Namespace),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) namespace_is_true,
        (BoolBinOp) namespace_equal,
        nullptr,
        nullptr,
        (UnaryOp)namespace_str,
        nullptr,
        (Trace) namespace_trace,
        (VoidUnaryOp) namespace_cleanup
};

Namespace *argon::object::NamespaceNew() {
    auto ns = ArObjectGCNew<Namespace>(&type_namespace_);

    if (ns != nullptr) {
        if (!HMapInit(&ns->hmap))
            Release((ArObject **) &ns);
    }

    return ns;
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
        argon::vm::Panic(OutOfMemoryError);
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
        argon::vm::Panic(OutOfMemoryError);
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

bool argon::object::NamespaceRemove(Namespace *ns, ArObject *key) {
    NsEntry *entry;

    if ((entry = (NsEntry *) HMapRemove(&ns->hmap, key)) != nullptr) {
        ReleaseEntryValue(entry, true);
        HMapEntryToFreeNode(&ns->hmap, entry);
        return true;
    }

    return false;
}