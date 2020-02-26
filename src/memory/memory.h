// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0
// EasterEgg: \qpu ;)

#ifndef ARGON_MEMORY_MEMORY_H_
#define ARGON_MEMORY_MEMORY_H_

#define ARGON_MEMORY_MINIMUM_POOL  16 // Minimum number of arenas, Argon WILL NEVER release this memory to the OS.

namespace argon::memory {
    void *Alloc(size_t size);

    void Free(void *ptr);
} // namespace argon::memory

#endif // !ARGON_MEMORY_MEMORY_H_
