// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#include <stratum/osmemory.h>

#if defined(_WIN32)

#include <Windows.h>

void *stratum::os::Alloc(size_t size) {
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void stratum::os::Free(void *ptr, size_t size) { VirtualFree(ptr, 0, MEM_RELEASE); }

#elif defined(__gnu_linux__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>
#include <cstdint>

void *stratum::os::Alloc(size_t size) {
    void *mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((uintptr_t) mem == -1) return nullptr;
    return mem;
}

void stratum::os::Free(void *ptr, size_t size) { munmap(ptr, size); }

#endif

