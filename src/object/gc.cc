// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <vm/runtime.h>

#include "gc.h"

using namespace argon::object;

/* GC variables */
GCGeneration generations[ARGON_OBJECT_GC_GENERATIONS] = {   // Generation queues
        {nullptr, 0, 0, 0, 550, 0},
        {nullptr, 0, 0, 0, 5,   0},
        {nullptr, 0, 0, 0, 5,   0},
};

GCHead *garbage = nullptr;                                  // Pointer to list of objects ready to be deleted

ArSize total_tracked = 0;                                   // Sum of the objects tracked in each generation
ArSize allocations = 0;
ArSize deallocations = 0;

std::atomic_bool enabled = true;
std::atomic_bool gc_requested = false;

std::mutex track_lck;                                       // GC lock
std::mutex garbage_lck;                                     // Garbage lock

void InsertObject(GCHead *obj, GCHead **list) {
    if (*list == nullptr) {
        obj->SetNext(nullptr);
        obj->prev = list;
    } else {
        obj->SetNext(*list);

        if (*list != nullptr)
            (*list)->prev = &obj->next;

        obj->prev = list;
    }

    *list = obj;
}

void RemoveObject(GCHead *head) {
    if (head->prev != nullptr)
        *head->prev = head->Next();
    if (head->Next() != nullptr)
        head->Next()->prev = head->prev;
}

inline void ResetStats(unsigned short generation) {
    if (generation == 0) {
        allocations = 0;
        deallocations = 0;
    } else
        generations[generation - 1].times = 0;

    generations[generation].count = 0;
    generations[generation].collected = 0;
    generations[generation].uncollected = 0;
}

void InitGCRefCount(GCHead *head, ArObject *obj) {
    head->ref = obj->ref_count.GetStrongCount();
    head->SetVisited(true);
}

void GCDecRef(ArObject *obj) {
    GCHead *head;

    if (obj != nullptr && GCIsTracking(obj)) {
        head = GCGetHead(obj);

        if (!head->IsVisited())
            InitGCRefCount(head, obj);

        head->ref--;
    }
}

void GCIncRef(ArObject *obj) {
    GCHead *head;

    if (obj != nullptr && GCIsTracking(obj)) {
        head = GCGetHead(obj);

        if (head->IsVisited()) {
            head->SetVisited(false);
            obj->type->trace(obj, GCIncRef);
        }

        head->ref++;
    }
}

void SearchRoots(GCGeneration *generation) {
    ArObject *obj;

    for (GCHead *cursor = generation->list; cursor != nullptr; cursor = cursor->Next()) {
        obj = cursor->GetObject();

        if (!cursor->IsVisited())
            InitGCRefCount(cursor, obj);

        obj->type->trace(obj, GCDecRef);
        generation->count++;
    }
}

void TraceRoots(GCGeneration *generation, GCHead **unreachable) {
    ArObject *obj;
    GCHead *tmp;

    for (GCHead *cursor = generation->list; cursor != nullptr; cursor = tmp) {
        tmp = cursor->Next();

        if (cursor->ref == 0) {
            cursor->SetFinalize(true);
            RemoveObject(cursor);
            InsertObject(cursor, unreachable);
            continue;
        }

        if (cursor->IsVisited()) {
            obj = cursor->GetObject();
            cursor->SetVisited(false);
            obj->type->trace(obj, GCIncRef);
        }
    }
}

void Trashing(GCHead *unreachable, GCGeneration *generation, unsigned short next_gen) {
    ArObject *obj;
    GCHead *tmp;

    for (GCHead *cursor = unreachable; cursor != nullptr; cursor = tmp) {
        tmp = cursor->Next();
        obj = cursor->GetObject();

        RemoveObject(cursor);

        // Check if objects are really unreachable
        if (cursor->ref == 0) {
            obj->type->cleanup(obj);

            generation->collected++;

            garbage_lck.lock();
            InsertObject(cursor, &garbage);
            total_tracked--;
            garbage_lck.unlock();
            continue;
        }

        cursor->SetFinalize(false);
        InsertObject(cursor, &generations[next_gen].list);
    }
}

ArObject *argon::object::GCNew(ArSize len) {
    auto obj = (GCHead *) memory::Alloc(sizeof(GCHead) + len);
    ArObject *tmp = nullptr;

    if (obj != nullptr) {
        obj->prev = nullptr;
        obj->next = nullptr;
        obj->ref = 0;

        tmp = (ArObject *) (((unsigned char *) obj) + sizeof(GCHead));
    }

    return tmp;
}

ArSize argon::object::Collect(unsigned short generation) {
    std::unique_lock lck(track_lck);

    GCHead *unreachable = nullptr;
    unsigned short next_gen;

    next_gen = (generation + 1) % ARGON_OBJECT_GC_GENERATIONS;
    if (next_gen == 0)
        next_gen = ARGON_OBJECT_GC_GENERATIONS - 1;

    ResetStats(generation);
    generations[generation].times++;

    if (generations[0].list == nullptr)
        return 0;

    // 1) Enumerate roots
    SearchRoots(&generations[generation]);

    // 2) Trace all objects reachable from roots
    TraceRoots(&generations[generation], &unreachable);

    // 3) Trash the unreachable objects
    Trashing(unreachable, &generations[generation], next_gen);

    generations[generation].uncollected = generations[generation].count - generations[generation].collected;
    return generations[generation].collected;
}

ArSize argon::object::Collect() {
    ArSize total_count = 0;

    for (int i = 0; i < ARGON_OBJECT_GC_GENERATIONS; i++)
        total_count += Collect(i);

    return total_count;
}

ArSize argon::object::STWCollect(unsigned short generation) {
    ArSize collected;

    argon::vm::StopTheWorld();
    collected = Collect(generation);
    argon::vm::StartTheWorld();

    Sweep();

    return collected;
}

ArSize argon::object::STWCollect() {
    ArSize collected;

    argon::vm::StopTheWorld();
    collected = Collect();
    argon::vm::StartTheWorld();

    Sweep();

    return collected;
}

bool argon::object::GCEnabled(bool enable) {
    bool status = atomic_exchange(&enabled, enable);
    return status;
}

bool argon::object::GCIsEnabled() {
    return enabled;
}

void argon::object::GCFree(ArObject *obj) {
    if (obj->ref_count.IsGcObject()) {
        if (GCGetHead(obj)->IsFinalized())
            return;

        UnTrack(obj);

        if (obj->type->cleanup != nullptr)
            obj->type->cleanup(obj);

        Release((ArObject *) obj->type);
        memory::Free(GCGetHead(obj));
    }
}

void argon::object::Sweep() {
    ArObject *obj;
    GCHead *cursor;
    GCHead *tmp;

    garbage_lck.lock();
    cursor = garbage;
    garbage = nullptr;
    garbage_lck.unlock();

    while (cursor != nullptr) {
        tmp = cursor;
        obj = cursor->GetObject();

        cursor = cursor->Next();

        Release((ArObject *) obj->type);
        memory::Free(tmp);
    }
}

void argon::object::Track(ArObject *obj) {
    auto *head = GCGetHead(obj);

    if (obj == nullptr || !obj->ref_count.IsGcObject())
        return;

    ThresholdCollect();

    track_lck.lock();
    if (!head->IsTracked()) {
        InsertObject(head, &generations[0].list);
        total_tracked++;
        allocations++;
    }
    track_lck.unlock();
}

void argon::object::ThresholdCollect() {
    bool requested = false;

    if (!enabled || allocations - deallocations < generations[0].threshold)
        return;

    if (!gc_requested.compare_exchange_strong(requested, true, std::memory_order_relaxed))
        return;

    argon::vm::StopTheWorld();

    Collect(0);

    if (generations[0].times >= generations[1].threshold)
        Collect(1);

    if (generations[1].times >= generations[2].threshold)
        Collect(2);

    gc_requested = false;

    argon::vm::StartTheWorld();

    Sweep();
}

void argon::object::UnTrack(ArObject *obj) {
    auto *head = GCGetHead(obj);

    if (obj == nullptr || !obj->ref_count.IsGcObject())
        return;

    track_lck.lock();
    if (GCIsTracking(obj)) {
        RemoveObject(head);
        total_tracked--;
        deallocations++;
    }
    track_lck.unlock();
}