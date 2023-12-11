// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/memory/memory.h>

#include <argon/vm/datatype/bufview.h>

using namespace argon::vm::datatype;
using namespace argon::vm::memory;

SharedBuffer *SharedBufferNew(ArSize cap, bool frozen) {
    auto *shared = (SharedBuffer *) Alloc(sizeof(SharedBuffer));

    if (shared != nullptr) {
        shared->counter = 1;

        if (frozen)
            shared->flags = SharedBufferFlags::FROZEN;

        shared->buffer = nullptr;
        shared->capacity = cap;

        if (cap != 0) {
            shared->buffer = (unsigned char *) Alloc(cap);
            if (shared->buffer == nullptr) {
                Free(shared);
                return nullptr;
            }
        }

        new(&shared->rwlock)argon::vm::sync::RecursiveSharedMutex();
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
    SharedBuffer *old = view->shared;
    SharedBuffer *tmp;

    if ((tmp = SharedBufferNew(view->length + count, false)) == nullptr)
        return false;

    // Acquire WriteLock on new SharedBuffer
    tmp->rwlock.lock();

    MemoryCopy(tmp->buffer, view->buffer, view->length);

    view->shared = tmp;
    view->buffer = tmp->buffer;

    // Release WriteLock on SharedBuffer
    old->rwlock.unlock();

    SharedBufferRelease(old);

    return true;
}

bool argon::vm::datatype::BufferViewAppendData(BufferView *view, const BufferView *other) {
    std::unique_lock view_lock(view->lock);

    view->shared->rwlock.lock();

    if (view != other)
        view->shared->rwlock.lock_shared();

    if (!BufferViewEnlarge(view, other->length)) {
        view->shared->rwlock.unlock();

        if (view != other)
            view->shared->rwlock.unlock_shared();

        return false;
    }

    memory::MemoryCopy(view->buffer + view->length, other->buffer, other->length);

    view->length += other->length;

    if (view != other)
        view->shared->rwlock.unlock_shared();

    view->shared->rwlock.unlock();

    return true;
}

bool argon::vm::datatype::BufferViewAppendData(BufferView *view, const unsigned char *buffer, ArSize length) {
    std::unique_lock view_lock(view->lock);

    view->shared->rwlock.lock();

    if (!BufferViewEnlarge(view, length)) {
        view->shared->rwlock.unlock();

        return false;
    }

    memory::MemoryCopy(view->buffer + view->length, buffer, length);

    view->length += length;

    view->shared->rwlock.unlock();

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

bool argon::vm::datatype::BufferViewHoldBuffer(BufferView *view, unsigned char *buffer, ArSize len,
                                               ArSize cap, bool frozen) {
    if ((view->shared = SharedBufferNew(0, frozen)) == nullptr)
        return false;

    if (buffer == nullptr) {
        len = 0;
        cap = 0;
    }

    view->shared->buffer = buffer;
    view->shared->capacity = cap;

    new(&view->lock)std::mutex();

    view->buffer = buffer;
    view->length = len;

    return true;
}

bool argon::vm::datatype::BufferViewInit(BufferView *view, ArSize capacity, bool frozen) {
    if ((view->shared = SharedBufferNew(capacity, frozen)) == nullptr)
        return false;

    new(&view->lock)std::mutex();
    view->buffer = view->shared->buffer;
    view->length = 0;

    return true;
}

void argon::vm::datatype::BufferViewDetach(BufferView *view) {
    SharedBufferRelease(view->shared);

    view->lock.~mutex();
    view->buffer = nullptr;
    view->length = 0;
}

void argon::vm::datatype::BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize length) {
    src->shared->Acquire();

    new(&dst->lock)std::mutex();

    dst->shared = src->shared;
    dst->buffer = src->buffer + start;
    dst->length = length;
}