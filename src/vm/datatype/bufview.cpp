// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/memory/memory.h>

#include "bufview.h"

using namespace argon::vm::datatype;
using namespace argon::vm::memory;

SharedBuffer *SharedBufferNew(ArSize cap) {
    auto *shared = (SharedBuffer *) Alloc(sizeof(SharedBuffer));

    if (shared != nullptr) {
        shared->counter = 1;

        shared->buffer = nullptr;
        shared->capacity = cap;

        if (cap != 0) {
            shared->buffer = (unsigned char *) Alloc(cap);
            if (shared->buffer == nullptr) {
                Free(shared);
                return nullptr;
            }
        }

        new(&shared->rwlock)std::shared_mutex();
    }

    return shared;
}

void SharedBufferRelease(SharedBuffer *shared) {
    if (shared->Release()) {
        shared->rwlock.~RecursiveSharedMutex();

        Free(shared->buffer);
        Free(shared);
    }
}

bool ViewEnlargeNew(BufferView *view, ArSize count) {
    SharedBuffer *tmp;

    if ((tmp = SharedBufferNew(view->length + count)) == nullptr)
        return false;

    MemoryCopy(tmp->buffer, view->buffer, view->length);
    view->buffer = tmp->buffer;

    SharedBufferRelease(view->shared);
    view->shared = tmp;

    return true;
}

bool argon::vm::datatype::BufferViewEnlarge(BufferView *view, ArSize count) {
    ArSize cap = view->shared->capacity + count;
    unsigned char *tmp;

    if (count == 0)
        cap = (view->shared->capacity + 1) + ((view->shared->capacity + 1) / 2);

    if (!view->shared->IsWritable())
        return ViewEnlargeNew(view, count);

    // IsSlice
    if (view->shared->buffer != view->buffer)
        MemoryCopy(view->shared->buffer, view->buffer, view->length);

    if (view->length + count >= view->shared->capacity) {
        if ((tmp = (unsigned char *) Realloc(view->shared->buffer, cap)) == nullptr)
            return false;

        view->shared->buffer = tmp;
        view->shared->capacity = cap;
    }

    view->buffer = view->shared->buffer;
    return true;
}

bool argon::vm::datatype::BufferViewHoldBuffer(BufferView *view, unsigned char *buffer, ArSize len, ArSize cap) {
    if ((view->shared = SharedBufferNew(0)) == nullptr)
        return false;

    if (buffer == nullptr) {
        len = 0;
        cap = 0;
    }

    view->shared->buffer = buffer;
    view->shared->capacity = cap;

    view->buffer = buffer;
    view->length = len;

    return true;
}

bool argon::vm::datatype::BufferViewInit(BufferView *view, ArSize capacity) {
    if ((view->shared = SharedBufferNew(capacity)) == nullptr)
        return false;

    view->buffer = view->shared->buffer;
    view->length = 0;

    return true;
}

bool argon::vm::datatype::BufferViewInit(BufferView *view, unsigned char *buffer, ArSize length, ArSize capacity) {
    if ((view->shared = SharedBufferNew(0)) == nullptr)
        return false;

    view->shared->buffer = buffer;
    view->shared->capacity = capacity;

    view->buffer = buffer;
    view->length = length;

    return true;
}

void argon::vm::datatype::BufferViewDetach(BufferView *view) {
    SharedBufferRelease(view->shared);
    view->buffer = nullptr;
    view->length = 0;
}

void argon::vm::datatype::BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize length) {
    src->shared->Acquire();
    dst->shared = src->shared;
    dst->buffer = src->buffer + start;
    dst->length = length;
}