// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "gc.h"
#include "object.h"

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
    *head->prev = head->next;
    if (head->next != nullptr)
        head->next->prev = head->prev;
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

bool argon::object::GCIsTracking(ArObject *obj) {
    if (obj->ref_count.IsGcObject())
        return GCGetHead(obj)->IsTracked();
    return false;
}

void GCDecRef(ArObject *obj) {
    if (GCIsTracking(obj)) {
        auto head = GCGetHead(obj);

        if (!head->IsVisited()) {
            head->ref = obj->ref_count.GetStrongCount();
            obj->ref_count.IncStrong(); // Required to break references cycle if the cleanup method will be called!
            head->SetVisited(true);
        }

        head->ref--;
    }
}

void GCIncRef(ArObject *obj) {
    if (GCIsTracking(obj)) {
        auto head = GCGetHead(obj);

        if (head->IsVisited()) {
            head->SetVisited(false);
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
            obj->type->trace(obj, GCIncRef);
        }
    }
}

void GC::Collect() {
    for (int i = 0; i < ARGON_OBJECT_GC_GENERATIONS; i++)
        this->Collect(i);
}

void GC::Collect(unsigned short generation) {
    GCHead unreachable{};

    if (this->generation_[generation].next == nullptr)
        return;

    // Enumerate roots
    this->SearchRoots(generation);

    // Trace objects reachable from roots
    this->TraceRoots(&unreachable, generation);
}

void argon::object::GC::Track(ArObject *obj) {
    auto head = GCGetHead(obj);

    if (!obj->ref_count.IsGcObject())
        return;

    this->track_lck.lock();
    if (!head->IsTracked())
        InsertObject(&this->generation_[0], head);
    this->track_lck.unlock();
}
