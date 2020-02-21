// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MEMORY_OSMEMORY_H_
#define ARGON_MEMORY_OSMEMORY_H_

namespace argon::memory::os {
    void *Alloc(size_t size);

    void Free(void *ptr, size_t size);

} // namespace argon::memory::os

#endif // !ARGON_MEMORY_OSMEMORY_H_
