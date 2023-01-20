// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BUFVIEW_H_
#define ARGON_VM_DATATYPE_BUFVIEW_H_

#include <atomic>

#include <vm/sync/rsm.h>

#include "objectdef.h"

namespace argon::vm::datatype {
    struct SharedBuffer {
        sync::RecursiveSharedMutex rwlock;

        std::atomic_long counter;

        unsigned char *buffer;
        ArSize capacity;

        bool IsWritable() const {
            return this->counter == 1;
        }

        bool Release() {
            return this->counter.fetch_sub(1) == 1;
        }

        void Acquire() {
            this->counter++;
        }
    };

    struct BufferView {
        SharedBuffer *shared;
        unsigned char *buffer;
        ArSize length;

        unsigned char *ReadableBufferLock() {
            this->shared->rwlock.lock_shared();
            return this->buffer;
        }

        unsigned char *WritableBufferLock() {
            this->shared->rwlock.lock();
            return this->buffer;
        }

        void ReadableRelease() {
            this->shared->rwlock.unlock_shared();
        }

        void WritableRelease() {
            this->shared->rwlock.unlock();
        }
    };

    bool BufferViewEnlarge(BufferView *view, ArSize count);

    bool BufferViewHoldBuffer(BufferView *view, unsigned char *buffer, ArSize len, ArSize cap);

    bool BufferViewInit(BufferView *view, ArSize capacity);

    bool BufferViewInit(BufferView *view, unsigned char *buffer, ArSize length, ArSize capacity);

    void BufferViewDetach(BufferView *view);

    void BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize length);
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_BUFVIEW_H_
