// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>
#include <argon/vm/datatype/error.h>

#include <argon/vm/memory/memory.h>

using namespace argon::vm::memory;

void *argon::vm::memory::Alloc(size_t size) {
    auto *mem = stratum::Alloc(size);

    if (mem == nullptr)
        Panic((datatype::ArObject *) datatype::error_oom);

    return mem;
}

void *argon::vm::memory::Calloc(size_t size) {
    auto *mem = stratum::Calloc(size);

    if (mem == nullptr)
        Panic((datatype::ArObject *) datatype::error_oom);

    return mem;
}

void argon::vm::memory::Free(void *ptr) {
    stratum::Free(ptr);
}

void *argon::vm::memory::Realloc(void *ptr, size_t size) {
    auto *mem = stratum::Realloc(ptr, size);

    if (mem == nullptr)
        Panic((datatype::ArObject *) datatype::error_oom);

    return mem;
}

// MALLOC WRAPPER

void *argon::vm::memory::Copy2Malloc(void *src, size_t size) {
    auto *buf = WMalloc(size);

    if(buf!= nullptr)
        argon::vm::memory::MemoryCopy(buf, src, size);

    return buf;
}

void *argon::vm::memory::WMalloc(size_t size) {
    void *ptr = malloc(size);

    if(ptr == nullptr)
        Panic((datatype::ArObject *) datatype::error_oom);

    return ptr;
}
