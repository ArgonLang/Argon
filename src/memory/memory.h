// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0
// EasterEgg: \qpu ;)

#ifndef ARGON_MEMORY_MEMORY_H_
#define ARGON_MEMORY_MEMORY_H_

#include <new>
#include <cstddef>

#define ARGON_MEMORY_MINIMUM_POOL       16 // Minimum number of arenas, Argon WILL NEVER release this memory to the OS.
#define ARGON_MEMORY_REALLOC_THRESHOLD  10

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

    int MemoryCompare(const void *ptr1, const void *ptr2, size_t num);

    void *MemoryCopy(void *dest, const void *src, size_t size);

    void *MemoryConcat(void *s1, size_t size1, void *s2, size_t size2);

    void *MemoryFind(const void *buf, unsigned char value, size_t size);

    void *MemorySet(void *dest, int val, size_t size);

    inline void *MemoryZero(void *dest, size_t size) { return MemorySet(dest, 0x00, size); }

    void *Realloc(void *ptr, size_t size);

    void InitializeMemory();

    void FinalizeMemory();
} // namespace argon::memory

#endif // !ARGON_MEMORY_MEMORY_H_
