// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0
// EasterEgg: \qpu ;)

#ifndef ARGON_MEMORY_MEMORY_H_
#define ARGON_MEMORY_MEMORY_H_

#include <new>
#include <cstddef>

#define ARGON_MEMORY_MINIMUM_POOL  16 // Minimum number of arenas, Argon WILL NEVER release this memory to the OS.

namespace argon::memory {
    void *Alloc(size_t size);

    template<typename T, typename ...Args>
    inline T *AllocObject(Args ...args) { return new(Alloc(sizeof(T))) T(args...); }

    void Free(void *ptr);

    template<typename T>
    inline void FreeObject(T *obj) {
        obj->~T();
        Free(obj);
    }

    void *MemoryCopy(void *dest, const void *src, size_t size);

    void *Realloc(void *ptr, size_t size);

    void InitializeMemory();

    void FinalizeMemory();
} // namespace argon::memory

#endif // !ARGON_MEMORY_MEMORY_H_
