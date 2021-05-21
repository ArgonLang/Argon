// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "gc.h"

using namespace argon::memory;
using namespace argon::object;

/* GC variables */
GCHead *generations[ARGON_OBJECT_GC_GENERATIONS] = {};  // Generation queues
GCHead *garbage = nullptr;                              // Pointer to list of objects ready to be deleted
GCStats stats[ARGON_OBJECT_GC_GENERATIONS] = {};        // Statistics
std::mutex track_lck;                                   // GC lock
std::mutex garbage_lck;                                 // Garbage lock

inline void InsertObject(GCHead **list, GCHead *obj) {
    if (*list == nullptr) {
        obj->next = nullptr;
        obj->prev = list;
    } else {
        obj->next = (*list);

        if ((*list) != nullptr)
            (*list)->prev = &obj->next;

        obj->prev = list;
    }

    *list = obj;
}

inline void RemoveObject(GCHead *head) {
    if (head->prev != nullptr)
        *head->prev = head->Next();
    if (head->Next() != nullptr)
        head->Next()->prev = head->prev;
}

inline void InitGCRefCount(GCHead *head, ArObject *obj) {
    head->ref = obj->ref_count.GetStrongCount();
    obj->ref_count.IncStrong(); // Required to break references cycle if the cleanup method will be called!
    head->SetVisited(true);
}

void GCIncRef(ArObject *obj) {
    if (GCIsTracking(obj)) {
        auto head = GCGetHead(obj);

        if (head->IsVisited()) {
            head->SetVisited(false);
            obj->ref_count.DecStrong(); // Acquired in first step (SearchRoots)
            obj->type->trace(obj, GCIncRef);
        }

        head->ref++;
    }
}

void GCDecRef(ArObject *obj) {
    if (GCIsTracking(obj)) {
        auto head = GCGetHead(obj);

        if (!head->IsVisited())
            InitGCRefCount(head, obj);

        head->ref--;
    }
}

void SearchRoots(unsigned short generation) {
    GCHead *cursor = generations[generation];
    ArObject *obj;

    while (cursor != nullptr) {
        obj = cursor->GetObject<ArObject>();

        if (!cursor->IsVisited())
            InitGCRefCount(cursor, obj);

        obj->type->trace(obj, GCDecRef);
        cursor = cursor->Next();

        stats[generation].count++;
    }
}

void TraceRoots(GCHead **unreachable, unsigned short generation) {
    GCHead *tmp;

    for (GCHead *cursor = generations[generation]; cursor != nullptr; cursor = tmp) {
        tmp = cursor->Next();

        if (cursor->ref == 0) {
            RemoveObject(cursor);
            InsertObject(unreachable, cursor);
            continue;
        }

        if (cursor->IsVisited()) {
            auto obj = cursor->GetObject<ArObject>();
            cursor->SetVisited(false);
            obj->ref_count.DecStrong(); // Acquired in first step (SearchRoots)
            obj->type->trace(obj, GCIncRef);
        }
    }
}

ArSize argon::object::Collect() {
    ArSize total_count = 0;

    for (int i = 0; i < ARGON_OBJECT_GC_GENERATIONS; i++)
        total_count += Collect(i);

    Sweep();

    return total_count;
}

ArSize argon::object::Collect(unsigned short generation) {
    GCHead *unreachable = nullptr;
    unsigned short next_gen = (generation + 1) % ARGON_OBJECT_GC_GENERATIONS;

    if (next_gen == 0)
        next_gen = ARGON_OBJECT_GC_GENERATIONS - 1;

    // Reset stats
    stats[generation].count = 0;
    stats[generation].collected = 0;
    stats[generation].uncollected = 0;

    if (generations[generation] == nullptr)
        return 0;

    // Enumerate roots
    SearchRoots(generation);

    // Trace objects reachable from roots
    TraceRoots(&unreachable, generation);

    // Check if object is really unreachable and break it's reference
    GCHead *tmp;
    for (GCHead *cursor = unreachable; cursor != nullptr; cursor = tmp) {
        auto obj = cursor->GetObject<ArObject>();
        tmp = cursor->next;

        RemoveObject(cursor);

        if (cursor->ref == 0) {
            obj->type->cleanup(obj);

            // Kill all weak reference (if any...)
            obj->ref_count.ClearWeakRef();

            garbage_lck.lock();
            InsertObject(&garbage, cursor);
            garbage_lck.unlock();

            stats[generation].collected++;

            continue;
        }

        InsertObject(&generations[next_gen], cursor);
    }

    stats[generation].uncollected = stats[generation].count - stats[generation].collected;

    return stats[generation].collected;
}

GCStats argon::object::GetStats(unsigned short generation) {
    return stats[generation];
}

void *argon::object::GCNew(ArSize len) {
    auto obj = (GCHead *) Alloc(sizeof(GCHead) + len);

    if (obj != nullptr) {
        obj->prev = nullptr;
        obj->next = nullptr;
        obj->ref = 0;

        obj = (GCHead *) (((unsigned char *) obj) + sizeof(GCHead));
    }

    Track((ArObject *) obj); // Inform the GC to track the object

    return obj;
}

void argon::object::Sweep() {
    GCHead *cursor;
    GCHead *tmp;

    garbage_lck.lock();
    cursor = garbage;
    garbage = nullptr;
    garbage_lck.unlock();

    while (cursor != nullptr) {
        tmp = cursor;
        cursor = cursor->next;
        tmp->GetObject<ArObject>()->ref_count.DecStrong();
        Free(tmp);
    }
}

void argon::object::Track(ArObject *obj) {
    auto head = GCGetHead(obj);

    if (!obj->ref_count.IsGcObject())
        return;

    track_lck.lock();
    if (!head->IsTracked())
        InsertObject(&generations[0], head);
    track_lck.unlock();
}

void argon::object::UnTrack(ArObject *obj) {
    auto head = GCGetHead(obj);

    track_lck.lock();
    if (GCIsTracking(obj))
        RemoveObject(head);
    track_lck.unlock();
}
