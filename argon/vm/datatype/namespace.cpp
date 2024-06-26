// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <shared_mutex>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/datatype/namespace.h>

using namespace argon::vm::datatype;

// Prototypes

bool NewEntry(Namespace *, ArObject *, ArObject *, AttributeFlag);

ArObject *namespace_compare(Namespace *self, ArObject *other, CompareMode mode) {
    auto *o = (Namespace *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    std::shared_lock self_lock(self->rwlock);
    std::shared_lock other_lock(o->rwlock);

    if (self->ns.length != o->ns.length)
        return BoolToArBool(false);

    for (auto *cursor = self->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        NSEntry *other_entry;

        o->ns.Lookup(cursor->key, &other_entry);

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
    NSEntry *entry;

    std::shared_lock _(ns->rwlock);

    if (!ns->ns.Lookup(key, &entry))
        return nullptr;

    if (entry != nullptr) {
        if (out_aprop != nullptr)
            *out_aprop = entry->value.properties;

        return entry->value.value.Get();
    }

    return nullptr;
}

ArObject *argon::vm::datatype::NamespaceLookup(Namespace *ns, const char *key, AttributeProperty *out_aprop) {
    auto *skey = StringNew(key);
    if (skey == nullptr)
        return nullptr;

    auto *ret = NamespaceLookup(ns, (ArObject *) skey, out_aprop);

    Release(skey);
    return ret;
}

bool argon::vm::datatype::NamespaceContains(Namespace *ns, ArObject *key, AttributeProperty *out_aprop) {
    NSEntry *entry;

    std::shared_lock _(ns->rwlock);

    if (!ns->ns.Lookup(key, &entry))
        return false;

    if (entry != nullptr) {
        if (out_aprop != nullptr)
            *out_aprop = entry->value.properties;

        return true;
    }

    return false;
}

bool argon::vm::datatype::NamespaceContains(Namespace *ns, const char *key,
                                            AttributeProperty *out_aprop, bool *out_exists) {
    auto *skey = StringNew(key);
    if (skey == nullptr)
        return false;

    *out_exists = NamespaceContains(ns, (ArObject *) skey, out_aprop);

    Release(skey);
    return true;
}

bool argon::vm::datatype::NamespaceMergePublic(Namespace *dest, Namespace *src) {
    std::unique_lock dst_lck(dest->rwlock);
    std::shared_lock src_lck(src->rwlock);

    for (auto *cursor = src->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (cursor->value.properties.IsPublic() && !cursor->value.properties.IsNonCopyable()) {
            bool ok = NewEntry(dest, cursor->key, cursor->value.value.Get(), cursor->value.properties.flags);
            if (!ok)
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

bool argon::vm::datatype::NamespaceNewSymbol(Namespace *ns, const char *key, const char *value, AttributeFlag aa) {
    auto *s_key = StringIntern(key);
    if (s_key == nullptr)
        return false;

    auto *s_value = StringNew(value);
    if (s_value == nullptr) {
        Release(s_key);

        return false;
    }

    auto ok = NamespaceNewSymbol(ns, (ArObject *) s_key, (ArObject *) s_value, aa);

    Release(s_key);
    Release(s_value);

    return ok;
}

bool argon::vm::datatype::NamespaceSet(Namespace *ns, ArObject *key, ArObject *value) {
    NSEntry *entry;

    assert(AR_GET_TYPE(key)->hash != nullptr);

    std::unique_lock _(ns->rwlock);

    if (!ns->ns.Lookup(key, &entry))
        return false;

    if (entry != nullptr) {
        entry->value.value.Store(value, !entry->value.properties.IsWeak());
        return true;
    }

    return false;
}

bool argon::vm::datatype::NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count) {
    ArSize idx = 0;

    if (count == 0)
        return true;

    std::unique_lock _(ns->rwlock);

    for (auto *cursor = ns->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if (idx >= count)
            break;

        if (cursor->value.properties.IsConstant())
            continue;

        cursor->value.value.Store(values[idx++], !cursor->value.properties.IsWeak());
    }

    return count <= idx;
}

bool NewEntry(Namespace *ns, ArObject *key, ArObject *value, AttributeFlag aa) {
    NSEntry *entry;

    if (!ns->ns.Lookup(key, &entry))
        return false;

    if (entry != nullptr) {
        entry->value.value.Store(value, ENUMBITMASK_ISFALSE(aa, AttributeFlag::WEAK));
        entry->value.properties.flags = aa;
        return true;
    }

    if ((entry = ns->ns.AllocHEntry()) == nullptr)
        return false;

    entry->key = IncRef(key);

    entry->value.value.Store(value, ENUMBITMASK_ISFALSE(aa, AttributeFlag::WEAK));
    entry->value.properties.flags = aa;

    if (!ns->ns.Insert(entry)) {
        Release(key);

        entry->value.value.Release();

        ns->ns.FreeHEntry(entry);
    }

    return true;
}

List *argon::vm::datatype::NamespaceKeysToList(Namespace *ns, AttributeFlag match) {
    std::shared_lock dst_lck(ns->rwlock);

    auto *list = ListNew(ns->ns.length);

    for (auto *cursor = ns->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if ((int) match == 0 || (cursor->value.properties.flags & match) == match) {
            bool ok = ListAppend(list, cursor->key);
            if (!ok) {
                Release(list);

                return nullptr;
            }
        }
    }

    return list;
}

Namespace *argon::vm::datatype::NamespaceNew() {
    auto *ns = MakeGCObject<Namespace>(&NamespaceType);

    if (ns != nullptr) {
        if (!ns->ns.Initialize()) {
            Release(ns);

            return nullptr;
        }

        new(&ns->rwlock)sync::RecursiveSharedMutex();

        memory::Track((ArObject*)ns);
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

Set *argon::vm::datatype::NamespaceKeysToSet(Namespace *ns, AttributeFlag match) {
    auto *set = SetNew();

    std::shared_lock dst_lck(ns->rwlock);

    for (auto *cursor = ns->ns.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        if ((int) match == 0 || (cursor->value.properties.flags & match) == match) {
            bool ok = SetAdd(set, cursor->key);
            if (!ok) {
                Release(set);

                return nullptr;
            }
        }
    }

    return set;
}

[[maybe_unused]] void argon::vm::datatype::NamespaceClear(Namespace *ns) {
    std::unique_lock _(ns->rwlock);

    ns->ns.Clear([](NSEntry *entry) {
        Release(entry->key);
        entry->value.value.Release();
    });
}