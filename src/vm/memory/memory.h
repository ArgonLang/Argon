// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_MEMORY_MEMORY_H_
#define ARGON_VM_MEMORY_MEMORY_H_

#include <cstddef>

#include <stratum/src/memory.h>
#include <stratum/src/memutil.h>

#define ARGON_VM_MEMORY_QUANTUM (STRATUM_QUANTUM)

namespace argon::vm::memory {
    const auto MemoryCompare = stratum::util::MemoryCompare;
    const auto MemoryCopy = stratum::util::MemoryCopy;
    const auto MemoryZero = stratum::util::MemoryZero;
    const auto MemoryInit = stratum::Initialize;
    const auto MemoryFinalize = stratum::Finalize;

    void *Alloc(size_t size);

    void *Calloc(size_t size);

    void *Realloc(void *ptr, size_t size);

    void Free(void *ptr);
}

#endif // !ARGON_VM_MEMORY_MEMORY_H_
