// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arobject.h>

#include <argon/vm/memory/gc.h>

using namespace argon::vm::datatype;
using namespace argon::vm::memory;

// GC variables
GCGeneration generations[kGCGenerations] = {   // Generation queues
        {nullptr, 0, 0, 0, 550, 0},
        {nullptr, 0, 0, 0, 5,   0},
        {nullptr, 0, 0, 0, 5,   0},
};

GCHead *garbage = nullptr;      // Pointer to list of objects ready to be deleted

ArSize total_tracked = 0;       // Sum of the objects tracked in each generation
ArSize allocations = 0;
ArSize deallocations = 0;

std::mutex track_lock;          // GC lock
std::mutex garbage_lock;        // Garbage lock

std::atomic_bool enabled = true;
std::atomic_bool gc_requested = false;

// Prototypes
inline void InitGCRefCount(GCHead *, const argon::vm::datatype::ArObject *);

void GCHeadInsert(GCHead **list, GCHead *head) {
    if (*list == nullptr) {
        head->SetNext(nullptr);
        head->prev = list;
    } else {
        head->SetNext(*list);

        if (*list != nullptr)
            (*list)->prev = &head->next;

        head->prev = list;
    }

    *list = head;
}

void GCHeadRemove(GCHead *head) {
    if (head->prev != nullptr)
        *head->prev = head->Next();
    if (head->Next() != nullptr)
        head->Next()->prev = head->prev;
}

void GCDecRef(argon::vm::datatype::ArObject *object) {
    GCHead *head = GCGetHead(object);

    if (head == nullptr || !head->IsTracked())
        return;

    if (!head->IsVisited())
        InitGCRefCount(head, object);

    head->ref--;
}

void GCIncRef(argon::vm::datatype::ArObject *object) {
    GCHead *head = GCGetHead(object);

    if (head == nullptr || !head->IsTracked())
        return;

    if (head->IsVisited()) {
        head->SetVisited(false);
        AR_GET_TYPE(object)->trace(object, GCIncRef);
    }

    head->ref++;
}

inline void InitGCRefCount(GCHead *head, const argon::vm::datatype::ArObject *object) {
    head->ref = AR_GET_RC(object).GetStrongCount();
    head->SetVisited(true);
}

void ResetStats(unsigned short generation) {
    if (generation == 0) {
        allocations = 0;
        deallocations = 0;
    } else
        generations[generation - 1].times = 0;

    generations[generation].count = 0;
    generations[generation].collected = 0;
    generations[generation].uncollected = 0;
}

void TraceRoots(GCGeneration *generation, GCHead **unreachable) {
    argon::vm::datatype::ArObject *object;
    GCHead *head;

    for (GCHead *cursor = generation->list; cursor != nullptr; cursor = head) {
        head = cursor->Next();

        if (cursor->ref == 0) {
            cursor->SetFinalize(true);
            GCHeadRemove(cursor);
            GCHeadInsert(unreachable, cursor);
            continue;
        }

        if (cursor->IsVisited()) {
            object = cursor->GetObject();

            cursor->SetVisited(false);

            AR_GET_TYPE(object)->trace(object, GCIncRef);
        }
    }
}

void Trashing(GCGeneration *generation, GCGeneration *next_generation, GCHead *unreachables) {
    argon::vm::datatype::ArObject *object;
    GCHead *head;

    for (GCHead *cursor = unreachables; cursor != nullptr; cursor = head) {
        object = cursor->GetObject();
        head = cursor->Next();

        GCHeadRemove(cursor);

        // Check if objects are really unreachable
        if (cursor->ref == 0) {
            AR_GET_TYPE(object)->dtor(object); // TODO: check return value...

            generation->collected++;

            std::unique_lock lock(garbage_lock);

            GCHeadInsert(&garbage, cursor);

            total_tracked--;

            continue;
        }

        cursor->SetFinalize(false);
        GCHeadInsert(&next_generation->list, cursor);
    }
}

void SearchRoots(GCGeneration *generation) {
    argon::vm::datatype::ArObject *object;

    for (GCHead *cursor = generation->list; cursor != nullptr; cursor = cursor->Next()) {
        object = cursor->GetObject();

        if (!cursor->IsVisited())
            InitGCRefCount(cursor, object);

        AR_GET_TYPE(object)->trace(object, GCDecRef);

        generation->count++;
    }
}

// PUBLIC

argon::vm::datatype::ArObject *argon::vm::memory::GCNew(ArSize length, bool track) {
    auto *head = (GCHead *) memory::Alloc(sizeof(GCHead) + length);
    if (head != nullptr) {
        memory::MemoryZero(head, sizeof(GCHead));

        if (track) {
            std::unique_lock lock(track_lock);

            GCHeadInsert(&(generations[0].list), head);
            total_tracked++;
            allocations++;
        }

        return (argon::vm::datatype::ArObject *) (((unsigned char *) head) + sizeof(GCHead));
    }

    return nullptr;
}

size_t argon::vm::memory::Collect(unsigned short generation) {
    GCHead *unreachable = nullptr;
    GCGeneration *selected;
    unsigned short next_gen;

    std::unique_lock lock(track_lock);

    if ((next_gen = (generation + 1) % kGCGenerations) == 0)
        next_gen = kGCGenerations - 1;

    ResetStats(generation);

    selected = generations + generation;

    selected->times++;

    if (generations[0].list == nullptr)
        return 0;

    // 1) Enumerate roots
    SearchRoots(selected);

    // 2) Trace all objects reachable from roots
    TraceRoots(selected, &unreachable);

    // 3) Trash the unreachable objects
    Trashing(selected, generations + next_gen, unreachable);

    selected->uncollected = selected->count - selected->collected;

    return selected->collected;
}

ArSize argon::vm::memory::Collect() {
    ArSize total_count = 0;

    for (unsigned short i = 0; i < kGCGenerations; i++)
        total_count += Collect(i);

    return total_count;
}

bool argon::vm::memory::GCEnable(bool enable) {
    return atomic_exchange(&enabled, enable);
}

bool argon::vm::memory::GCIsEnabled() {
    return enabled;
}

GCHead *argon::vm::memory::GCGetHead(datatype::ArObject *object) {
    if (object == nullptr || !AR_GET_RC(object).IsGcObject())
        return nullptr;

    return GC_GET_HEAD(object);
}

void argon::vm::memory::GCFree(datatype::ArObject *object) {
    auto *head = GCGetHead(object);

    if(head == nullptr)
        return;

    if(head->IsTracked()) {
        AR_GET_RC(object).DecStrong(nullptr);
        return;
    }

    if (AR_GET_RC(object).DecStrong(nullptr)) {
        if (AR_GET_TYPE(object)->dtor != nullptr)
            AR_GET_TYPE(object)->dtor(object);

        MonitorDestroy(object);

        memory::Free(head);
    }
}

void argon::vm::memory::Sweep() {
    GCHead *cursor;

    std::unique_lock lock(garbage_lock);

    cursor = garbage;
    garbage = nullptr;

    lock.unlock();

    while (cursor != nullptr) {
        auto *obj = cursor->GetObject();
        auto *tmp = cursor;

        cursor = cursor->Next();

        Release(AR_GET_TYPE(obj));

        MonitorDestroy(obj);

        memory::Free(tmp);
    }
}

void argon::vm::memory::ThresholdCollect() {
    bool desired = false;

    if (!enabled || (allocations - deallocations) < generations[0].threshold)
        return;

    if (!gc_requested.compare_exchange_strong(desired, true, std::memory_order_relaxed))
        return;

    Collect(0);

    for (unsigned short i = 1; i < kGCGenerations; i++)
        if (generations[i - 1].times >= generations[i].threshold)
            Collect(i);

    gc_requested = false;

    Sweep();
}

void argon::vm::memory::Track(datatype::ArObject *object) {
    auto *head = GCGetHead(object);

    if (head == nullptr)
        return;

    ThresholdCollect();

    std::unique_lock lock(track_lock);
    if (!head->IsTracked()) {
        GCHeadInsert(&(generations[0].list), head);
        total_tracked++;
        allocations++;
    }
}

void argon::vm::memory::TrackIf(datatype::ArObject *track, datatype::ArObject *gc_object) {
    const auto *head = GCGetHead(gc_object);

    if (head != nullptr)
        Track(track);
}
