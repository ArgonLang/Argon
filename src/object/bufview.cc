// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "bufview.h"

using namespace argon::memory;
using namespace argon::object;

SharedBuffer *SharedBufferNew(ArSize cap) {
    auto *shared = (SharedBuffer *) Alloc(sizeof(SharedBuffer));

    if (shared != nullptr) {
        shared->lock = 0;
        shared->counter = 1;
        shared->cap = cap;

        shared->buffer = nullptr;

        if (cap == 0)
            return shared;

        shared->buffer = (unsigned char *) Alloc(cap);
        if (shared->buffer == nullptr) {
            Free(shared);
            return nullptr;
        }
    }

    return shared;
}

void SharedBufferRelease(SharedBuffer *shared) {
    if (shared->Release()) {
        Free(shared->buffer);
        Free(shared);
    }
}

bool ViewEnlargeNew(BufferView *view, ArSize count) {
    SharedBuffer *tmp;

    if ((tmp = SharedBufferNew(view->len + count)) == nullptr)
        return false;

    MemoryCopy(tmp->buffer, view->buffer, view->len);
    view->buffer = tmp->buffer;

    SharedBufferRelease(view->shared);
    view->shared = tmp;

    return true;
}

bool argon::object::BufferViewEnlarge(BufferView *view, ArSize count) {
    unsigned char *tmp;
    ArSize cap = count > 1 ? view->shared->cap + count : (view->shared->cap + 1) + ((view->shared->cap + 1) / 2);

    if (!view->shared->IsWritable())
        return ViewEnlargeNew(view, count);

    if (view->shared->buffer == nullptr)
        cap = 20; // TO DEFINE!

    // IsSlice
    if (view->shared->buffer != view->buffer) {
        MemoryCopy(view->shared->buffer, view->buffer, view->len);
        view->buffer = view->shared->buffer;
    }

    if (view->len + count >= view->shared->cap) {
        if ((tmp = (unsigned char *) Realloc(view->shared->buffer, cap)) == nullptr)
            return false;

        view->shared->buffer = tmp;
        view->shared->cap = cap;
        view->buffer = view->shared->buffer;
    }

    return true;
}

bool argon::object::BufferViewInit(BufferView *view, ArSize capacity) {
    if ((view->shared = SharedBufferNew(capacity)) == nullptr)
        return false;

    view->buffer = view->shared->buffer;
    view->len = 0;

    return true;
}

bool argon::object::BufferViewHoldBuffer(BufferView *view, unsigned char *buffer, ArSize len, ArSize cap) {
    if ((view->shared = SharedBufferNew(0)) == nullptr)
        return false;

    view->shared->buffer = buffer;
    view->shared->cap = cap;

    view->buffer = buffer;
    view->len = len;

    return true;
}

void argon::object::BufferViewDetach(BufferView *view) {
    SharedBufferRelease(view->shared);
    view->buffer = nullptr;
    view->len = 0;
}

void argon::object::BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize len) {
    src->shared->Increment();
    dst->shared = src->shared;
    dst->buffer = src->buffer + start;
    dst->len = len;
}