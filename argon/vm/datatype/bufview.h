// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BUFVIEW_H_
#define ARGON_VM_DATATYPE_BUFVIEW_H_

#include <atomic>
#include <shared_mutex>

#include <argon/vm/sync/rsm.h>

#include <argon/vm/datatype/objectdef.h>

namespace argon::vm::datatype {
    struct SharedBuffer {
        sync::RecursiveSharedMutex rwlock;

        std::atomic_long counter;

        unsigned char *buffer;
        ArSize capacity;

        [[nodiscard]] bool IsWritable() const {
            return this->counter == 1;
        }

        bool Release() {
            return this->counter.fetch_sub(1) == 1;
        }

        void Acquire() {
            // It is used to verify that no one has started writing operations!
            std::shared_lock _(this->rwlock);

            this->counter++;
        }
    };

    struct BufferView {
        std::mutex lock;

        SharedBuffer *shared;

        unsigned char *buffer;
        ArSize length;
    };

    bool BufferViewAppendData(BufferView *view, const BufferView *other);

    bool BufferViewAppendData(BufferView *view, const unsigned char *buffer, ArSize length);

    bool BufferViewEnlarge(BufferView *view, ArSize count);

    bool BufferViewHoldBuffer(BufferView *view, unsigned char *buffer, ArSize len, ArSize cap);

    bool BufferViewInit(BufferView *view, ArSize capacity);

    void BufferViewDetach(BufferView *view);

    void BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize length);
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_BUFVIEW_H_
