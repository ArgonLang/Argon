// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_MEMORY_MEMORY_H_
#define ARGON_VM_MEMORY_MEMORY_H_

#include <cstddef>

#include <stratum/src/memory.h>

#define ARGON_VM_MEMORY_QUANTUM (STRATUM_QUANTUM)

namespace argon::vm::memory {
    void *Alloc(size_t size);

    void *Realloc(void *ptr, size_t size);

    void Free(void *ptr);
}

#endif // !ARGON_VM_MEMORY_MEMORY_H_
