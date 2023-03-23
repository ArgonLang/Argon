// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#ifndef STRATUM_MEMUTIL_H_
#define STRATUM_MEMUTIL_H_

#include <cstddef>

namespace stratum::util {
    /**
     * @brief Compares the first num bytes of the block of memory.
     *
     * Compares the first num bytes of the block of memory pointed by \p ptr1
     * to the first \p num bytes pointed by \p ptr2 returning zero if they all match or a value different
     * from zero representing which is greater if they do not.
     *
     * @param ptr1 Pointer to block of memory.
     * @param ptr2 Pointer to block of memory.
     * @param num Number of bytes to compare.
     * @return Returns an integral value indicating the relationship between the content of the memory blocks.
     */
    int MemoryCompare(const void *ptr1, const void *ptr2, size_t num);

    /**
     * @brief Concatenates the contents of two memory blocks in the target block.
     *
     * If the destination block is smaller than the sum of \p size1 + \p size2 the content copied to \p dest
     * will be truncated to the value of \p sized.
     *
     * @param dest Pointer to destination block of memory.
     * @param sized Size of destination block of memory.
     * @param s1 Pointer to block of memory.
     * @param size1 Size of first block of memory.
     * @param s2 Pointer to block of memory.
     * @param size2 Size of second block of memory.
     * @return Pointer to \p dest memory block.
     */
    void *MemoryConcat(void *dest, size_t sized, const void *s1, size_t size1, const void *s2, size_t size2);

    /**
     * @brief Copies the values of source memory block to destination memory block.
     *
     * Copies the values of \p size bytes from the location pointed to by \p src directly to the memory
     * block pointed to by \p dest.
     *
     * @param dest Pointer to the destination array where the content is to be copied.
     * @param src Pointer to the source of data to be copied.
     * @param size Number of bytes to copy.
     * @return Destination is returned.
     */
    void *MemoryCopy(void *dest, const void *src, size_t size);

    /**
     * @brief Search for a value in a block of memory.
     *
     * Searches within the first \p num bytes of the block of memory pointed by \p ptr
     * for the first occurrence of \p value (interpreted as an unsigned char), and returns a pointer to it.
     *
     * @param buf
     * @param value
     * @param size
     * @return
     */
    void *MemoryFind(const void *buf, unsigned char value, size_t num);

    /**
     * @brief Sets bytes of memory block to a specified value.
     *
     * Sets the first \p num bytes of the block of memory pointed by \p ptr to the specified \p value.
     *
     * @param ptr Pointer to the block of memory to fill.
     * @param value Value to be set.
     * @param num Number of bytes to be set to the value.
     * @return \p ptr is returned.
     */
    void *MemorySet(void *ptr, int value, size_t num);

    /**
     * @brief Sets bytes of memory block to value zero.
     *
     * Sets the first \p num bytes of the block of memory pointed by \p ptr to zero.
     *
     * @param ptr Pointer to the block of memory to fill.
     * @param num Number of bytes to be set to the value.
     * @return \p ptr is returned.
     */
    inline void *MemoryZero(void *ptr, size_t num) { return MemorySet(ptr, 0x00, num); }
} // namespace stratum::util


#endif // !STRATUM_MEMUTIL_H_
