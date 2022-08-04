// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "memory.h"

using namespace argon::vm::memory;

void *argon::vm::memory::Alloc(size_t size) {
    return stratum::Alloc(size);
}

void *argon::vm::memory::Calloc(size_t size) {
    return stratum::Calloc(size);
}

void argon::vm::memory::Free(void *ptr) {
    stratum::Free(ptr);
}

void *argon::vm::memory::Realloc(void *ptr, size_t size) {
    return stratum::Realloc(ptr, size);
}