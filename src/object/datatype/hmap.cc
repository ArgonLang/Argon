// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "error.h"
#include "hash_magic.h"
#include "string.h"
#include "hmap.h"

using namespace argon::memory;
using namespace argon::object;

#define CHECK_HASHABLE(obj, ret)                                                        \
do {                                                                                    \
if(!IsHashable(obj)) {                                                                  \
    ErrorFormat(&error_unhashable, "unhashable type: '%s'", AR_TYPE_NAME(obj));  \
    return ret;                                                                         \
}} while(false)

bool HMapResize(HMap *hmap) {
    HEntry **new_map;
    size_t new_cap;
    size_t hash;

    if (((float) hmap->len + 1) / hmap->cap < ARGON_OBJECT_HMAP_LOAD_FACTOR)
        return true;

    new_cap = hmap->cap + (hmap->cap / ARGON_OBJECT_HMAP_MUL_FACTOR);

    new_map = (HEntry **) Realloc(hmap->map, new_cap * sizeof(void *));

    if (new_map == nullptr) {
        argon::vm::Panic(OutOfMemoryError);
        return false;
    }

    MemoryZero(new_map + hmap->cap, (new_cap - hmap->cap) * sizeof(void *));

    for (size_t i = 0; i < hmap->cap; i++) {
        for (HEntry *prev = nullptr, *cur = new_map[i], *next; cur != nullptr; cur = next) {
            hash = Hash(cur->key) % new_cap;
            next = cur->next;

            if (hash == i) {
                prev = cur;
                continue;
            }

            cur->next = new_map[hash];
            new_map[hash] = cur;
            if (prev != nullptr)
                prev->next = next;
            else
                new_map[i] = next;
        }
    }

    hmap->map = new_map;
    hmap->cap = new_cap;

    return true;
}

void AppendIterItem(HMap *hmap, HEntry *entry) {
    if (hmap->iter_begin == nullptr) {
        hmap->iter_begin = entry;
        hmap->iter_end = entry;
        return;
    }

    entry->iter_next = nullptr;
    entry->iter_prev = hmap->iter_end;
    hmap->iter_end->iter_next = entry;
    hmap->iter_end = entry;
}

void RemoveIterItem(HMap *hmap, HEntry *entry) {
    if (entry->iter_prev != nullptr)
        entry->iter_prev->iter_next = entry->iter_next;
    else
        hmap->iter_begin = entry->iter_next;

    if (entry->iter_next != nullptr)
        entry->iter_next->iter_prev = entry->iter_prev;
    else
        hmap->iter_end = entry->iter_prev;

    entry->iter_next = nullptr;
    entry->iter_prev = nullptr;
}

bool argon::object::HMapInit(HMap *hmap) {
    hmap->map = (HEntry **) Alloc(ARGON_OBJECT_HMAP_INITIAL_SIZE * sizeof(void *));

    if (hmap->map != nullptr) {
        hmap->free_node = nullptr;
        hmap->iter_begin = nullptr;
        hmap->iter_end = nullptr;

        hmap->cap = ARGON_OBJECT_HMAP_INITIAL_SIZE;
        hmap->len = 0;

        MemoryZero(hmap->map, hmap->cap * sizeof(void *));

        return true;
    }

    return false;
}

bool argon::object::HMapInsert(HMap *hmap, HEntry *entry) {
    size_t index;

    if (!HMapResize(hmap))
        return false;

    CHECK_HASHABLE(entry->key, false);

    index = Hash(entry->key) % hmap->cap;

    entry->next = hmap->map[index];
    hmap->map[index] = entry;
    AppendIterItem(hmap, entry);
    hmap->len++;

    return true;
}

HEntry *argon::object::HMapLookup(HMap *hmap, ArObject *key) {
    size_t index;

    CHECK_HASHABLE(key, nullptr);

    index = Hash(key) % hmap->cap;

    for (HEntry *cur = hmap->map[index]; cur != nullptr; cur = cur->next)
        if (AR_EQUAL(key, cur->key))
            return cur;

    return nullptr;
}

HEntry *argon::object::HMapLookup(HMap *hmap, const char *key, size_t len) {
    size_t index = HashBytes((const unsigned char *) key, len) % hmap->cap;

    for (HEntry *cur = hmap->map[index]; cur != nullptr; cur = cur->next)
        if (AR_TYPEOF(cur->key, type_string_)) {
            if (StringEq((String *) cur->key, (const unsigned char *) key, len))
                return cur;
        }

    return nullptr;
}

HEntry *argon::object::HMapRemove(HMap *hmap, ArObject *key) {
    size_t index;

    CHECK_HASHABLE(key, nullptr);

    index = Hash(key) % hmap->cap;

    for (HEntry *cur = hmap->map[index]; cur != nullptr; cur = cur->next) {
        if (AR_EQUAL(key, cur->key)) {
            RemoveIterItem(hmap, cur);
            hmap->len--;
            return cur;
        }
    }

    return nullptr;
}

void argon::object::HMapFinalize(HMap *hmap, HMapCleanFn clean_fn) {
    HEntry *tmp;

    for (HEntry *cur = hmap->iter_begin; cur != nullptr; cur = tmp) {
        tmp = cur->iter_next;
        Release(cur->key);

        if (clean_fn != nullptr)
            clean_fn(cur);

        Free(cur);
    }

    for (HEntry *cur = hmap->free_node; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        Free(cur);
    }

    Free(hmap->map);
}

void argon::object::HMapRemove(HMap *hmap, HEntry *entry) {
    RemoveIterItem(hmap, entry);
    HMapEntryToFreeNode(hmap, entry);
    hmap->len--;
}

#undef CHECK_HASHABLE