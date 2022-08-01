// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#include "memutil.h"

int stratum::util::MemoryCompare(const void *ptr1, const void *ptr2, size_t num) {
    auto *p1 = (const unsigned char *) ptr1;
    auto *p2 = (const unsigned char *) ptr2;

    if (ptr1 == nullptr && ptr2 == nullptr)
        return 0;
    else if (ptr1 == nullptr)
        return *p2;
    else if (ptr2 == nullptr)
        return *p1;

    while (num-- != 0) {
        if (*p1++ != *p2++)
            return *--p1 - *--p2;
    }

    return 0;
}

void *stratum::util::MemoryConcat(void *dest, size_t sized, const void *s1, size_t size1,
                                  const void *s2, size_t size2) {
    if (size1 > sized)
        size1 = sized;

    if (size2 > sized - size1)
        size2 = sized - size1;

    if (dest != nullptr) {
        MemoryCopy(dest, s1, size1);
        MemoryCopy(((unsigned char *) dest) + size1, s2, size2);
    }

    return dest;
}

void *stratum::util::MemoryCopy(void *dest, const void *src, size_t size) {
    auto d = (unsigned char *) dest;
    auto s = (const unsigned char *) src;

    while (size--)
        *d++ = *s++;

    return d;
}

void *stratum::util::MemoryFind(const void *buf, unsigned char value, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (((unsigned char *) buf)[i] == value)
            return (void *) (((unsigned char *) buf) + i);
    }

    return nullptr;
}

void *stratum::util::MemorySet(void *ptr, int value, size_t num) {
    auto p = (unsigned char *) ptr;

    while (num-- > 0)
        *p++ = value;

    return ptr;
}
