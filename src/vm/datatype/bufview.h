// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BUFVIEW_H_
#define ARGON_VM_DATATYPE_BUFVIEW_H_

#include <atomic>

#include "objectdef.h"

namespace argon::vm::datatype {
    struct SharedBuffer {
        std::atomic_long counter;

        unsigned char *buffer;
        ArSize capacity;

        void Acquire() {
            this->counter++;
        }

        bool IsWritable() const {
            return this->counter == 1;
        }

        bool Release() {
            return this->counter.fetch_sub(1) == 1;
        }
    };

    struct BufferView {
        SharedBuffer *shared;
        unsigned char *buffer;
        ArSize length;
    };

    bool BufferViewEnlarge(BufferView *view, ArSize count);

    bool BufferViewInit(BufferView *view, ArSize capacity);

    bool BufferViewInit(BufferView *view, unsigned char *buffer, ArSize length, ArSize capacity);

    void BufferViewDetach(BufferView *view);

    void BufferViewInit(BufferView *dst, BufferView *src, ArSize start, ArSize length);
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_BUFVIEW_H_
