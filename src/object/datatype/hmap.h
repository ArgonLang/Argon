// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_HMAP_H_
#define ARGON_OBJECT_HMAP_H_

#include <atomic>
#include <cstring>

#include <memory/memory.h>

#include <object/arobject.h>
#include <object/rwlock.h>

#define ARGON_OBJECT_HMAP_INITIAL_SIZE  12
#define ARGON_OBJECT_HMAP_MAX_FREE_LEN  24
#define ARGON_OBJECT_HMAP_LOAD_FACTOR   0.75f
#define ARGON_OBJECT_HMAP_MUL_FACTOR    (ARGON_OBJECT_HMAP_LOAD_FACTOR * 2)

#define HMAP_ITERATOR(name, next, peek)                                 \
const IteratorSlots name {                                              \
        nullptr,                                                        \
        (UnaryOp) next,                                                 \
        (UnaryOp) peek,                                                 \
        nullptr                                                         \
};                                                                      \
const TypeInfo type_##name##_ = {                                       \
        TYPEINFO_STATIC_INIT,                                           \
        #name,                                                          \
        nullptr,                                                        \
        sizeof(HMapIterator),                                           \
        TypeInfoFlags::BASE,                                            \
        nullptr,                                                        \
        (VoidUnaryOp) HMapIteratorCleanup,                              \
        (Trace) HMapIteratorTrace,                                      \
        (CompareOp) HMapIteratorCompare,                                \
        (BoolUnaryOp) HMapIteratorIsTrue,                               \
        nullptr,                                                        \
        (UnaryOp) HMapIteratorStr,                                      \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        &name,                                                          \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr,                                                        \
        nullptr                                                         \
}

namespace argon::object {

    using HMapCleanFn = void (*)(struct HEntry *);

    struct HEntry {
        std::atomic_int ref;
        HEntry *next;

        HEntry *iter_next;
        HEntry *iter_prev;

        ArObject *key;
    };

    struct HMap {
        RWLock lock;

        HEntry **map;
        HEntry *free_node;
        HEntry *iter_begin;
        HEntry *iter_end;

        ArSize cap;
        ArSize len;

        ArSize free_count;
        ArSize free_max;
    };

    struct HMapIterator : ArObject {
        SimpleLock lock;
        ArObject *obj;

        HMap *map;
        HEntry *current;

        bool reversed;
    };

    ArObject *HMapIteratorCompare(HMapIterator *self, ArObject *other, CompareMode mode);

    ArObject *HMapIteratorNew(const TypeInfo *type, ArObject *iterable, HMap *map, bool reversed);

    ArObject *HMapIteratorStr(HMapIterator *self);

    inline bool HMapIteratorIsTrue(HMapIterator *self){return true;}

    inline bool HMapIteratorIsValid(HMapIterator *self) {
        return self->current != nullptr && self->current->key != nullptr;
    }

    void HMapIteratorCleanup(HMapIterator *iter);

    void HMapIteratorNext(HMapIterator *self);

    inline void HMapIteratorTrace(HMapIterator *iter, VoidUnaryOp trace) { trace(iter->obj); }

    // *** HMap

    bool HMapInit(HMap *hmap, ArSize freenode_max);

    inline bool HMapInit(HMap *hmap) { return HMapInit(hmap, ARGON_OBJECT_HMAP_MAX_FREE_LEN); }

    bool HMapInsert(HMap *hmap, HEntry *entry);

    HEntry *HMapLookup(HMap *hmap, ArObject *key);

    HEntry *HMapLookup(HMap *hmap, const char *key, ArSize len);

    inline HEntry *HMapLookup(HMap *hmap, const char *key) {
        return HMapLookup(hmap, key, strlen(key));
    }

    HEntry *HMapRemove(HMap *hmap, ArObject *key);

    template<typename T>
    typename std::enable_if<std::is_base_of<HEntry, T>::value, T>::type *
    HMapFindOrAllocNode(HMap *hmap) {
        T *entry;

        if (hmap->free_node == nullptr) {
            if ((entry = (T *) argon::memory::Alloc(sizeof(T))) != nullptr) {
                argon::memory::MemoryZero(entry, sizeof(T));
                entry->ref = 1;
            }
            return entry;
        }

        entry = (T *) hmap->free_node;
        hmap->free_node = entry->next;
        hmap->free_count--;
        return entry;
    }

    void HMapClear(HMap *hmap, HMapCleanFn clean_fn);

    void HMapEntryToFreeNode(HMap *hmap, HEntry *entry);

    void HMapFinalize(HMap *hmap, HMapCleanFn clean_fn);

    void HMapRemove(HMap *hmap, HEntry *entry);

} // namespace argon::object

#endif // !ARGON_OBJECT_HMAP_H_
