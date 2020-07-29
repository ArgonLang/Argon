// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "namespace.h"
#include "nil.h"

using namespace argon::object;
using namespace argon::memory;

void FreeOrReplace(NsEntry *entry, ArObject *value) {
    if (entry->info.IsWeak()) {
        entry->ref.DecWeak();
        if (value != nullptr)
            entry->ref = value->ref_count.IncWeak();
        return;
    }

    Release(entry->obj);
    if (value != nullptr) {
        IncRef(value);
        entry->obj = value;
    }
}

bool CheckSize(Namespace *ns) {
    NsEntry **new_ns;
    size_t new_cap;

    if (((float) ns->len + 1) / ns->cap < ARGON_OBJECT_NS_LOAD_FACTOR)
        return true;

    new_cap = ns->cap + (ns->cap / ARGON_OBJECT_NS_MUL_FACTOR);

    new_ns = (NsEntry **) Realloc(ns->ns, new_cap * sizeof(NsEntry *));
    if (new_ns == nullptr)
        return false;

    for (size_t i = ns->cap; i < new_cap; i++)
        new_ns[i] = nullptr;

    for (size_t i = 0; i < new_cap; i++) {
        for (NsEntry *prev = nullptr, *cur = new_ns[i], *next; cur != nullptr; cur = next) {
            size_t hash = cur->key->type->hash(cur->key) % new_cap;
            next = cur->next;

            if (hash == i) {
                prev = cur;
                continue;
            }

            cur->next = new_ns[i];
            new_ns[i] = cur->next;
            if (prev != nullptr)
                prev->next = next;
            else
                new_ns[i] = next;
        }
    }

    ns->ns = new_ns;
    ns->cap = new_cap;

    return true;
}

void namespace_cleanup(ArObject *obj) {
    auto map = (Namespace *) obj;

    for (size_t i = 0; i < map->cap; i++) {
        NsEntry *next;
        for (NsEntry *cursor = map->ns[i]; cursor != nullptr; cursor = next) {
            next = cursor->next;
            FreeOrReplace(cursor, nullptr);
            Free(cursor);
        }
    }

    Free(map->ns);
}

void namespace_trace(Namespace *self, VoidUnaryOp trace) {
    for (size_t i = 0; i < self->cap; i++)
        for (NsEntry *cursor = self->ns[i]; cursor != nullptr; cursor = cursor->next)
            trace(cursor->obj);
}

const TypeInfo type_namespace_ = {
        (const unsigned char *) "namespace",
        sizeof(Namespace),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (Trace) namespace_trace,
        namespace_cleanup
};

void AppendIterItem(Namespace *ns, NsEntry *entry) {
    if (ns->iter_begin == nullptr) {
        ns->iter_begin = entry;
        ns->iter_end = entry;
        return;
    }

    entry->iter_next = nullptr;
    entry->iter_prev = ns->iter_end;
    ns->iter_end->iter_next = entry;
    ns->iter_end = entry;
}

void RemoveIterItem(Namespace *ns, NsEntry *entry) {
    if (entry->iter_prev != nullptr)
        entry->iter_prev->iter_next = entry->iter_next;
    else
        ns->iter_begin = entry->iter_next;

    if (entry->iter_next != nullptr)
        entry->iter_next->iter_prev = entry->iter_prev;
    else
        ns->iter_end = entry->iter_prev;
}

Namespace *argon::object::NamespaceNew() {
    auto ns = ArObjectNewGC<Namespace>(&type_namespace_);

    if (ns != nullptr) {
        if ((ns->ns = (NsEntry **) Alloc(ARGON_OBJECT_NS_INITIAL_SIZE * sizeof(NsEntry *))) == nullptr) {
            Release(ns);
            return nullptr;
        }

        ns->iter_begin = nullptr;
        ns->iter_end = nullptr;
        ns->cap = ARGON_OBJECT_NS_INITIAL_SIZE;
        ns->len = 0;

        for (size_t i = 0; i < ns->cap; i++)
            ns->ns[i] = nullptr;
    }

    return ns;
}

bool argon::object::NamespaceNewSymbol(Namespace *ns, PropertyInfo info, ArObject *key, ArObject *value) {
    NsEntry *entry;
    size_t index;

    if (!CheckSize(ns))
        return false;

    index = key->type->hash(key) % ns->cap;
    for (entry = ns->ns[index]; entry != nullptr; entry = entry->next) {
        if (key->type->equal(key, entry->key)) {
            FreeOrReplace(entry, nullptr); // Release value
            break;
        }
    }

    if (entry == nullptr) {
        entry = (NsEntry *) Alloc(sizeof(NsEntry));
        assert(entry != nullptr);// TODO: nomem

        IncRef(key);
        entry->key = key;

        entry->next = ns->ns[index];
        ns->ns[index] = entry;

        AppendIterItem(ns, entry);

        ns->len++;
    }

    entry->info = info;

    if (entry->info.IsWeak()) {
        entry->ref = value->ref_count.IncWeak();
        return true;
    }

    IncRef(value);
    entry->obj = value;
    return true;
}

bool argon::object::NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value) {
    size_t index = key->type->hash(key) % ns->cap;

    for (NsEntry *cur = ns->ns[index]; cur != nullptr; cur = cur->next) {
        if (key->type->equal(key, cur->key)) {
            FreeOrReplace(cur, value);
            return true;
        }
    }

    return false;
}

bool argon::object::NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info) {
    size_t index = key->type->hash(key) % ns->cap;

    for (NsEntry *cur = ns->ns[index]; cur != nullptr; cur = cur->next)
        if (key->type->equal(key, cur->key)) {
            if (info != nullptr)
                *info = cur->info;
            return true;
        }

    return false;
}

ArObject *argon::object::NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info) {
    size_t index = key->type->hash(key) % ns->cap;

    for (NsEntry *cur = ns->ns[index]; cur != nullptr; cur = cur->next) {
        if (key->type->equal(key, cur->key)) {
            if (info != nullptr)
                *info = cur->info;

            if (cur->info.IsWeak())
                return ReturnNil(cur->ref.GetObject());
            else
                return cur->obj->ref_count.GetObject();
        }
    }

    return nullptr;
}

void argon::object::NamespaceRemove(Namespace *ns, ArObject *key) {
    size_t index = key->type->hash(key) % ns->cap;

    NsEntry *prev = nullptr;
    for (NsEntry *cur = ns->ns[index]; cur != nullptr; cur = cur->next) {
        if (key->type->equal(key, cur->key)) {
            Release(cur->key);
            FreeOrReplace(cur, nullptr);
            RemoveIterItem(ns, cur);

            if (prev == nullptr)
                ns->ns[index] = cur->next;
            else
                prev->next = cur->next;

            Free(cur);
            ns->len--;
            break;
        }
        prev = cur;
    }
}