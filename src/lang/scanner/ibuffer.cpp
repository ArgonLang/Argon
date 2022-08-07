// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/memory/memory.h>

#include "ibuffer.h"

using namespace argon::lang::scanner;

InputBuffer::~InputBuffer() {
    if (this->release)
        argon::vm::memory::Free(this->start);
}

bool InputBuffer::AppendInput(const unsigned char *buffer, int length) {
    unsigned char *tmp;
    long t_cur;
    long t_inp;
    long t_end;

    if (this->start == nullptr) {
        this->start = (unsigned char *) argon::vm::memory::Alloc(length);

        if (this->start == nullptr)
            return false;

        this->cur = this->start;
        this->end = this->start + length;

        this->inp = (unsigned char *) argon::vm::memory::MemoryCopy(this->start, buffer, length);

        return true;
    }

    t_cur = this->cur - this->start;
    t_inp = this->inp - this->start;
    t_end = this->end - this->start;

    tmp = (unsigned char *) argon::vm::memory::Realloc(this->start, t_end + length);
    if (tmp == nullptr)
        return false;

    this->start = tmp;
    this->cur = tmp + t_cur;
    this->end = tmp + (t_end + length);

    this->inp = (unsigned char *) argon::vm::memory::MemoryCopy(tmp + t_inp, buffer, length);

    return true;
}

int InputBuffer::Peek(bool advance) {
    int chr;

    if (this->cur != this->inp) {
        chr = *this->cur;

        if (this->cur >= this->end) {
            // Circular buffer
            chr = *this->start;
        }

        if (advance) {
            if (this->cur >= this->end) {
                this->cur = this->start;
                return chr;
            }

            this->cur++;
        }

        return chr;
    }

    return 0;
}

int InputBuffer::ReadFile(FILE *fd, int length) {
    unsigned long tread = 0;
    unsigned long read;
    long rsize;

    // Build circular-buffer
    if (this->start == nullptr) {
        this->start = (unsigned char *) argon::vm::memory::Alloc(length);
        if (this->start == nullptr)
            return -1;

        this->cur = this->start;
        this->inp = this->start;
        this->end = this->start + length;
    }

    // Forward reading
    rsize = this->end - this->inp;
    if (rsize > 0) {
        read = std::fread(this->inp, 1, rsize, fd);
        tread = read;

        if (ferror(fd) != 0 || read == 0 && feof(fd) != 0)
            return 0;

        this->inp += read;
    }

    rsize = this->cur - this->start;
    if (rsize > 0) {
        read = std::fread(this->start, 1, rsize, fd);
        tread += read;

        if (ferror(fd) != 0 || read == 0 && feof(fd) != 0)
            return 0;

        this->inp = this->start + read;

        if (this->cur == this->end)
            this->cur = this->start;
    }

    return (int) tread;
}