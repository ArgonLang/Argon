// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "arstring.h"
#include "nil.h"
#include "namespace.h"

using namespace argon::vm::datatype;

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

TypeInfo NamespaceType = {
        AROBJ_HEAD_INIT_TYPE,
        "Namespace",
        nullptr,
        nullptr,
        sizeof(Namespace),
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
const TypeInfo *argon::vm::datatype::type_namespace_ = &NamespaceType;

ArObject *argon::vm::datatype::NamespaceLookup(Namespace *ns, ArObject *key, AttributeProperty *out_aprop) {
    NSEntry  *entry = ns->ns.Lookup(key);

    if(entry != nullptr){
        if(out_aprop != nullptr)
            *out_aprop = entry->value.properties;

        return entry->value.value.Get();
    }

    return nullptr;
}

bool argon::vm::datatype::NamespaceContains(Namespace *ns, ArObject *key, AttributeProperty *out_aprop) {
    const NSEntry *entry = ns->ns.Lookup(key);

    if (entry != nullptr) {
        *out_aprop = entry->value.properties;
        return true;
    }

    return false;
}

bool argon::vm::datatype::NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, AttributeFlag aa) {
    assert(AR_GET_TYPE(key)->hash != nullptr);

    if (value == nullptr)
        value = (ArObject *) IncRef(Nil);

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

    if ((entry = ns->ns.Lookup(key)) != nullptr) {
        entry->value.value.Store(value);
        return true;
    }

    return false;
}

Namespace *argon::vm::datatype::NamespaceNew() {
    auto *ns = MakeGCObject<Namespace>(&NamespaceType);

    if (ns != nullptr && !ns->ns.Initialize())
        Release(ns);

    return ns;
}

void argon::vm::datatype::NamespaceClear(Namespace *ns) {
    ns->ns.Clear([](NSEntry *entry) {
        Release(entry->key);
        entry->value.value.Release();
    });
}