// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "arstring.h"
#include "boolean.h"
#include "nil.h"

#include "namespace.h"

using namespace argon::vm::datatype;

// Prototypes

bool NewEntry(Namespace *, ArObject *, ArObject *, AttributeFlag);

ArObject *namespace_compare(Namespace *self, ArObject *other, CompareMode mode) {
    auto *o = (Namespace *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    // *** WARNING ***
    // Why std::unique_lock? See vm/sync/rsm.h
    std::unique_lock self_lock(self->rwlock);
    std::unique_lock other_lock(o->rwlock);

    if (self->ns.length != o->ns.length)
        return BoolToArBool(false);

    for (auto *cursor = self->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        auto *other_entry = o->ns.Lookup(cursor->key);

        if (other_entry == nullptr)
            return BoolToArBool(false);

        auto *s_value = cursor->value.value.Get();
        auto *o_value = other_entry->value.value.Get();

        auto ok = Equal(s_value, o_value);

        Release(s_value);
        Release(o_value);

        if (!ok)
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

bool namespace_dtor(Namespace *self) {
    self->ns.Finalize([](HEntry<ArObject, PropertyStore> *entry) {
        Release(entry->key);
        entry->value.value.Release();
    });

    self->rwlock.~RecursiveSharedMutex();

    return true;
}

bool namespace_is_true(Namespace *self) {
    std::shared_lock _(self->rwlock);

    return self->ns.length > 0;
}

void namespace_trace(Namespace *self, Void_UnaryOp trace) {
    std::shared_lock _(self->rwlock);

    for (auto *cursor = self->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        trace(cursor->key);

        if (!cursor->value.properties.IsWeak())
            trace(cursor->value.value.GetRawReference());
    }
}

TypeInfo NamespaceType = {
        AROBJ_HEAD_INIT_TYPE,
        "Namespace",
        nullptr,
        nullptr,
        sizeof(Namespace),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) namespace_dtor,
        (TraceOp) namespace_trace,
        nullptr,
        (Bool_UnaryOp) namespace_is_true,
        (CompareOp) namespace_compare,
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
const TypeInfo *argon::vm::datatype::type_namespace_ = &NamespaceType;

ArObject *argon::vm::datatype::NamespaceLookup(Namespace *ns, ArObject *key, AttributeProperty *out_aprop) {
    std::shared_lock _(ns->rwlock);

    NSEntry *entry = ns->ns.Lookup(key);

    if (entry != nullptr) {
        if (out_aprop != nullptr)
            *out_aprop = entry->value.properties;

        return entry->value.value.Get();
    }

    return nullptr;
}

bool argon::vm::datatype::NamespaceContains(Namespace *ns, ArObject *key, AttributeProperty *out_aprop) {
    std::shared_lock _(ns->rwlock);

    const NSEntry *entry = ns->ns.Lookup(key);

    if (entry != nullptr) {
        *out_aprop = entry->value.properties;
        return true;
    }

    return false;
}

bool argon::vm::datatype::NamespaceMergePublic(Namespace *dest, Namespace *src) {
    std::unique_lock dst_lck(dest->rwlock);
    std::shared_lock src_lck(src->rwlock);

    for (auto *cursor = src->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (cursor->value.properties.IsPublic()) {
            if (!NewEntry(dest, cursor->key, cursor->value.value.Get(), cursor->value.properties.flags))
                return false;
        }
    }

    return true;
}

bool argon::vm::datatype::NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, AttributeFlag aa) {
    assert(AR_GET_TYPE(key)->hash != nullptr);

    if (value == nullptr)
        value = (ArObject *) IncRef(Nil);

    std::unique_lock _(ns->rwlock);

    return NewEntry(ns, key, value, aa);
}

bool argon::vm::datatype::NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, AttributeFlag aa) {
    auto *str = StringIntern(key);
    if (str == nullptr)
        return false;

    auto ok = NamespaceNewSymbol(ns, (ArObject *) str, value, aa);

    Release(str);

    return ok;
}

bool argon::vm::datatype::NamespaceSet(Namespace *ns, ArObject *key, ArObject *value) {
    NSEntry *entry;

    assert(AR_GET_TYPE(key)->hash != nullptr);

    std::unique_lock _(ns->rwlock);

    if ((entry = ns->ns.Lookup(key)) != nullptr) {
        entry->value.value.Store(value);
        return true;
    }

    return false;
}

bool argon::vm::datatype::NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count) {
    ArSize idx = 0;

    std::unique_lock _(ns->rwlock);

    for (auto *cursor = ns->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (idx >= count)
            return false;

        if (cursor->value.properties.IsConstant())
            continue;

        cursor->value.value.Store(values[idx++]);
    }

    return true;
}

bool NewEntry(Namespace *ns, ArObject *key, ArObject *value, AttributeFlag aa) {
    NSEntry *entry = ns->ns.Lookup(key);

    if (entry != nullptr) {
        entry->value.value.Store(value, ENUMBITMASK_ISTRUE(aa, AttributeFlag::WEAK));
        entry->value.properties.flags = aa;
        return true;
    }

    if ((entry = ns->ns.AllocHEntry()) == nullptr)
        return false;

    entry->key = IncRef(key);

    entry->value.value.Store(value, ENUMBITMASK_ISTRUE(aa, AttributeFlag::WEAK));
    entry->value.properties.flags = aa;

    if (!ns->ns.Insert(entry)) {
        Release(key);

        entry->value.value.Release();

        ns->ns.FreeHEntry(entry);
    }

    return true;
}

Namespace *argon::vm::datatype::NamespaceNew() {
    auto *ns = MakeGCObject<Namespace>(&NamespaceType, true);

    if (ns != nullptr) {
        if (!ns->ns.Initialize()) {
            Release(ns);

            return nullptr;
        }

        new(&ns->rwlock)sync::RecursiveSharedMutex();
    }

    return ns;
}

Namespace *argon::vm::datatype::NamespaceNew(Namespace *ns, AttributeFlag ignore) {
    Namespace *ret;

    if ((ret = NamespaceNew()) == nullptr)
        return nullptr;

    for (auto *cursor = ns->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if ((int) ignore == 0 || (int) (cursor->value.properties.flags & ignore) == 0) {
            auto *value = cursor->value.value.Get();

            if (!NamespaceNewSymbol(ret, cursor->key, value, cursor->value.properties.flags)) {
                Release(value);
                Release(ret);

                return nullptr;
            }

            Release(value);
        }
    }

    return ret;
}

void argon::vm::datatype::NamespaceClear(Namespace *ns) {
    std::unique_lock _(ns->rwlock);

    ns->ns.Clear([](NSEntry *entry) {
        Release(entry->key);
        entry->value.value.Release();
    });
}