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

void argon::object::GC::Track(ArObject *obj) {
    auto head = GCGetHead(obj);

    if (!obj->ref_count.IsGcObject())
        return;

    this->generation[0].lock.lock();
    if (!head->IsTracked())
        InsertObject(&this->generation[0].tracked, head);
    this->generation[0].lock.unlock();
}
