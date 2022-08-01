// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#ifndef STRATUM_OSMEMORY_H_
#define STRATUM_OSMEMORY_H_

#include <cstddef>

namespace stratum::os {
    /**
     * @brief Reserve a region of free pages.
     *
     * @param size Memory size required.
     * @return A void * to the beginning of the allocated block.
     */
    void *Alloc(size_t size);

    /**
     * @brief Release a page region of \p size previously allocated by a call to stratum::os::Alloc.
     * @param ptr Pointer to memory block to be freed.
     * @param size \p Size of region to release.
     */
    void Free(void *ptr, size_t size);
} // namespace stratum::os

#endif // !STRATUM_OSMEMORY_H_
