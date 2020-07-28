// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "gc.h"

using namespace argon::memory;
using namespace argon::object;

inline void InsertObject(GCHead *head, GCHead *next) {
    next->next = head->next;

    if (next->next != nullptr)
        next->next->prev = &next->next;

    head->next = next;
    next->prev = &head->next;
}

inline void RemoveObject(GCHead *head) {
    *head->prev = head->Next();
    if (head->Next() != nullptr)
        head->Next()->prev = head->prev;
}

void *argon::object::GCNew(size_t len) {
    auto obj = (GCHead *) Alloc(sizeof(GCHead) + len);

    if (obj != nullptr) {
        obj->prev = nullptr;
        obj->next = nullptr;
        obj->ref = 0;

        obj = (GCHead *) (((unsigned char *) obj) + sizeof(GCHead));
    }

    return obj;
}

inline void InitGCRefCount(GCHead *head, ArObject *obj){
    head->ref = obj->ref_count.GetStrongCount();
    obj->ref_count.IncStrong(); // Required to break references cycle if the cleanup method will be called!
    head->SetVisited(true);
}

void GCDecRef(ArObject *obj) {
    if (GCIsTracking(obj)) {
        auto head = GCGetHead(obj);

        if (!head->IsVisited())
            InitGCRefCount(head, obj);

        head->ref--;
    }
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

void GC::SearchRoots(unsigned short generation) {
    GCHead *cursor = this->generation_[generation].next;
    ArObject *obj;

    while (cursor != nullptr) {
        obj = cursor->GetObject<ArObject>();

        if(!cursor->IsVisited())
            InitGCRefCount(cursor, obj);

        obj->type->trace(obj, GCDecRef);
        cursor = cursor->Next();
    }
}

void GC::TraceRoots(GCHead *unreachable, unsigned short generation) {
    GCHead *tmp;

    for (GCHead *cursor = this->generation_[generation].next; cursor != nullptr; cursor = tmp) {
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

void GC::Collect() {
    for (int i = 0; i < ARGON_OBJECT_GC_GENERATIONS; i++)
        this->Collect(i);
    this->Sweep();
}

void GC::Collect(unsigned short generation) {
    GCHead unreachable{};

    if (this->generation_[generation].next == nullptr)
        return;

    // Enumerate roots
    this->SearchRoots(generation);

    // Trace objects reachable from roots
    this->TraceRoots(&unreachable, generation);

    // Check if object is really unreachable and break it's reference
    GCHead *tmp;
    for (GCHead *cursor = unreachable.next; cursor != nullptr; cursor = tmp) {
        auto obj = cursor->GetObject<ArObject>();
        tmp = cursor->next;

        RemoveObject(cursor);

        if (cursor->ref == 0) {
            obj->type->cleanup(obj);

            // Kill all weak reference (if any...)
            obj->ref_count.ClearWeakRef();

            this->garbage_lck_.lock();
            InsertObject(&this->garbage_, cursor);
            this->garbage_lck_.unlock();

            continue;
        }

        InsertObject(&this->generation_[generation], cursor);
    }
}

void argon::object::GC::Track(ArObject *obj) {
    auto head = GCGetHead(obj);

    if (!obj->ref_count.IsGcObject())
        return;

    this->track_lck_.lock();
    if (!head->IsTracked())
        InsertObject(&this->generation_[0], head);
    this->track_lck_.unlock();
}

void GC::Sweep() {
    GCHead *cursor;
    GCHead *tmp;

    this->garbage_lck_.lock();
    cursor = this->garbage_.next;
    this->garbage_.next = nullptr;
    this->garbage_lck_.unlock();

    while (cursor != nullptr) {
        tmp = cursor;
        cursor = cursor->next;
        tmp->GetObject<ArObject>()->ref_count.DecStrong();
        Free(tmp);
    }
}

void GC::UnTrack(struct ArObject *obj) {
    auto head = GCGetHead(obj);

    this->track_lck_.lock();
    if (GCIsTracking(obj))
        RemoveObject(head);
    this->track_lck_.unlock();
}
