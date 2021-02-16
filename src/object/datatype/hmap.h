// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_HMAP_H_
#define ARGON_OBJECT_HMAP_H_

#include <cstring>

#include <memory/memory.h>

#include <object/arobject.h>

#define ARGON_OBJECT_HMAP_INITIAL_SIZE   12
#define ARGON_OBJECT_HMAP_LOAD_FACTOR    0.75f
#define ARGON_OBJECT_HMAP_MUL_FACTOR     (ARGON_OBJECT_HMAP_LOAD_FACTOR * 2)

namespace argon::object {

    using HMapCleanFn = void (*)(struct HEntry *);

    struct HEntry {
        HEntry *next;

        HEntry *iter_next;
        HEntry *iter_prev;

        ArObject *key;
    };

    struct HMap {
        HEntry **map;
        HEntry *free_node;
        HEntry *iter_begin;
        HEntry *iter_end;

        size_t cap;
        size_t len;
    };

    struct HMapIterator : ArObject {
        ArObject *obj;

        HMap *map;
        HEntry *current;

        size_t used;
        bool reversed;
    };

    ArObject *HMapIteratorNew(const TypeInfo *type, ArObject *iterable, HMap *map, bool reversed);

    ArObject *HMapIteratorStr(HMapIterator *self);

    bool HMapIteratorEqual(HMapIterator *self, ArObject *other);

    inline bool HMapIteratorHasNext(HMapIterator *iter) { return iter->current != nullptr; }

    inline void HMapIteratorReset(HMapIterator *iter) {
        iter->current = iter->reversed ? iter->map->iter_end : iter->map->iter_begin;
    }

    inline void HMapIteratorTrace(HMapIterator *iter, VoidUnaryOp trace) { trace(iter->obj); }

    inline void HMapIteratorCleanup(HMapIterator *iter) { Release(&iter->obj); }

    bool HMapInit(HMap *hmap);

    bool HMapInsert(HMap *hmap, HEntry *entry);

    HEntry *HMapLookup(HMap *hmap, ArObject *key);

    HEntry *HMapLookup(HMap *hmap, const char *key, size_t len);

    inline HEntry *HMapLookup(HMap *hmap, const char *key) {
        return HMapLookup(hmap, key, strlen(key));
    }

    HEntry *HMapRemove(HMap *hmap, ArObject *key);

    template<typename T>
    typename std::enable_if<std::is_base_of<HEntry, T>::value, T>::type *
    HMapFindOrAllocNode(HMap *hmap) {
        T *entry;

        if (hmap->free_node == nullptr) {
            if ((entry = (T *) argon::memory::Alloc(sizeof(T))) != nullptr)
                argon::memory::MemoryZero(entry, sizeof(T));
            return entry;
        }

        entry = (T *) hmap->free_node;
        hmap->free_node = entry->next;
        return entry;
    }

    inline void HMapEntryToFreeNode(HMap *hmap, HEntry *entry) {
        entry->next = hmap->free_node;
        hmap->free_node = entry;
    }

    void HMapFinalize(HMap *hmap, HMapCleanFn clean_fn);

    void HMapRemove(HMap *hmap, HEntry *entry);

} // namespace argon::object

#endif // !ARGON_OBJECT_HMAP_H_
