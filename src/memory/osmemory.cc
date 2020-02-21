// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "osmemory.h"

using namespace argon::memory::os;

#if defined(_WIN32)

#include <Windows.h>

inline void *Alloc(size_t size) { return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }

inline void Free(void *ptr, size_t size) { VirtualFree(ptr, 0, MEM_RELEASE); }

#elif defined(__gnu_linux__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>

inline void *Alloc(size_t size) {
    void *mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem < -1) return nullptr;
    return mem;
}

inline void Free(void *ptr, size_t size) { munmap(ptr, size); }

#endif

