// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BUFVIEW_H_
#define ARGON_OBJECT_BUFVIEW_H_

#include <atomic>

#include "rwlock.h"
#include "arobject.h"

namespace argon::object {
    struct SharedBuffer {
        RWLock lock;

        std::atomic_long counter;
        unsigned char *buffer;
        ArSize cap;

        [[nodiscard]] bool IsWritable() const {
            return this->counter == 1;
        }

        bool Release() {
            return this->counter.fetch_sub(1) == 1;
        }

        void Increment() {
            this->counter++;
        }
    };

    struct BufferView {
        SharedBuffer *shared;
        unsigned char *buffer;
        ArSize len;
    };

    bool BufferViewEnlarge(BufferView *view, ArSize count);

    bool BufferViewInit(BufferView *view, ArSize capacity);

    bool BufferViewHoldBuffer(BufferView *view, unsigned char *buffer, ArSize len, ArSize cap);

    void BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize len);

    void BufferViewDetach(BufferView *view);
} // namespace argon::object

#endif // !ARGON_OBJECT_BUFVIEW_H_
