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
    void *Alloc(size_t size) noexcept;

    void Free(void *ptr);

    template<typename T, typename ...Args>
    inline T *AllocObject(Args ...args) {
        auto mem = Alloc(sizeof(T));
        T *obj = nullptr;

        if (mem != nullptr) {
            try {
                obj = new(mem) T(args...);
            } catch (...) {
                Free(mem);
                throw;
            }
        }

        return obj;
    }

    template<typename T>
    inline void FreeObject(T *obj) {
        obj->~T();
        Free(obj);
    }

    void *MemoryCopy(void *dest, const void *src, size_t size);

    void *MemoryConcat(void *s1, size_t size1, void *s2, size_t size2);

    void *MemorySet(void *dest, int val, size_t size);

    inline void *MemoryZero(void *dest, size_t size) { return MemorySet(dest, 0x00, size); }

    void *Realloc(void *ptr, size_t size);

    void InitializeMemory();

    void FinalizeMemory();
} // namespace argon::memory

#endif // !ARGON_MEMORY_MEMORY_H_
