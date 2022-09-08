// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <vm/memory/memory.h>

#include "sbuffer.h"

using namespace argon::lang::scanner;

StoreBuffer::~StoreBuffer() {
    argon::vm::memory::Free(this->buffer_);
}

bool StoreBuffer::Enlarge(size_t increase) {
    if (this->buffer_ == nullptr) {
        this->buffer_ = (unsigned char *) argon::vm::memory::Alloc(increase + 1);
        if (this->buffer_ == nullptr)
            return false;

        this->cursor_ = this->buffer_;
        this->end_ = this->buffer_ + increase;
    }

    if (this->cursor_ >= this->end_) {
        auto newsz = ((this->end_ + 1) - this->buffer_) + increase;
        auto tmp = (unsigned char *) argon::vm::memory::Realloc(this->buffer_, newsz);
        if (tmp == nullptr)
            return false;

        this->cursor_ = tmp + (this->cursor_ - this->buffer_);
        this->buffer_ = tmp;
        this->end_ = tmp + (newsz - 1);
    }

    return true;
}

bool StoreBuffer::PutChar(unsigned char chr) {
    do {
        if (this->cursor_ < this->end_) {
            *this->cursor_ = chr;
            this->cursor_++;
            return true;
        }

        if (!this->Enlarge(8))
            return false;

    } while (true);
}

bool StoreBuffer::PutCharRepeat(unsigned char chr, int n) {
    if (!this->Enlarge(n))
        return false;

    while (n > 0) {
        *this->cursor_ = chr;
        this->cursor_++;
        n--;
    }

    return true;
}

bool StoreBuffer::PutString(const unsigned char *str, size_t length) {
    if (!this->Enlarge(length + 8))
        return false;

    this->cursor_ = (unsigned char *) argon::vm::memory::MemoryCopy(this->cursor_, str, length);
    return true;
}

unsigned int StoreBuffer::GetBuffer(unsigned char **buffer) {
    auto length = (unsigned int) (this->cursor_ - this->buffer_);

    if (this->buffer_ == nullptr) {
        *buffer = nullptr;
        return 0;
    }

    assert(this->cursor_ < (this->end_ + 1));

    *this->cursor_ = '\0';
    *buffer = this->buffer_;

    this->buffer_ = nullptr;
    this->cursor_ = nullptr;
    this->end_ = nullptr;

    return length;
}