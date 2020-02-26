// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MEMORY_ARENA_H_
#define ARGON_MEMORY_ARENA_H_

#include <cstdint>

#define ARGON_MEMORY_PAGE_SIZE          4096                                        // Assume page size of 4096 bytes
#define ARGON_MEMORY_ARENA_SIZE         (((unsigned int)256) << (unsigned int)10)   // 256 KiB

#define ARGON_MEMORY_QUANTUM            8
#define ARGON_MEMORY_BLOCK_MAX_SIZE     512
#define ARGON_MEMORY_CLASSES            (ARGON_MEMORY_BLOCK_MAX_SIZE / ARGON_MEMORY_QUANTUM)

/*
 * Argon memory layout:
 *                                 +--+
 * +-------------------------------+  |
 * | POOL  | POOL  | POOL  | POOL  |  |
 * | HEADER| HEADER| HEADER| HEADER|  |
 * +-------+-------+-------+-------|  |
 * | BLOCK |       | BLOCK |       |  |
 * |       | BLOCK +-------+   B   |  |
 * +-------+       | BLOCK |   I   |  | A
 * | BLOCK +-------+-------+   G   |  | R
 * |       |       | BLOCK |       |  | E . . .
 * +-------+ BLOCK +-------+   B   |  | N
 * | BLOCK |       | BLOCK |   L   |  | A
 * |       +-------+-------+   O   |  |
 * +-------+       | BLOCK |   C   |  |
 * +-------+ BLOCK +-------+   K   |  |
 * | ARENA |       | BLOCK |       |  |
 * +-------------------------------+  |
 *                                 +--+
 */
namespace argon::memory {
    using Uintptr = uintptr_t;

    inline void *AlignDown(const void *ptr, const size_t sz) {
        return (void *) (((Uintptr) ptr) & ~(sz - 1));
    }

    inline void *AlignUp(const void *ptr, const size_t sz) {
        return (void *) (((Uintptr) ptr + sz) & ~(sz - 1));
    }

    inline size_t SizeToPoolClass(size_t size) {
        return (((size + (ARGON_MEMORY_QUANTUM - 1)) & ~((size_t) ARGON_MEMORY_QUANTUM - 1)) / ARGON_MEMORY_QUANTUM) -
               1;
    }

    inline size_t ClassToSize(size_t clazz) {
        return ARGON_MEMORY_QUANTUM + (ARGON_MEMORY_QUANTUM * clazz);
    }

    struct alignas(ARGON_MEMORY_QUANTUM)Arena {
        /* Total pools in the arena */
        unsigned int pools = 0;

        /* Number of free pools in the arena */
        unsigned int free = 0;

        /* Pointer to linked-list of available pools */
        struct Pool *pool = nullptr;

        /* Pointers to next and prev arena, arenas are linked between each other with a doubly linked-list */
        struct Arena *next = nullptr;
        struct Arena *prev = nullptr;
    };

    struct alignas(ARGON_MEMORY_QUANTUM)Pool {
        /* Pointer to Arena */
        Arena *arena = nullptr;

        /* Total blocks in pool */
        unsigned short blocks = 0;

        /* Free blocks in pool */
        unsigned short free = 0;

        /* Size of single memory block */
        unsigned short blocksz = 0;

        /* Pointer to linked-list of available blocks */
        void *block = nullptr;

        /* Pointers to next and prev pool of a specific size-class */
        struct Pool *next = nullptr;
        struct Pool *prev = nullptr;
    };

    Arena *AllocArena();

    Pool *AllocPool(Arena *arena, size_t clazz);

    void FreeArena(Arena *arena);

    void FreePool(Pool *pool);

    void *AllocBlock(Pool *pool);

    void FreeBlock(Pool *pool, void *block);

} // namespace argon::memory

#endif // !ARGON_MEMORY_ARENA_H_
