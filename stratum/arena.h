// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#ifndef STRATUM_ARENA_H_
#define STRATUM_ARENA_H_

#include <cstddef>
#include <cstdint>

/**
 * @brief Size of single page.
 *
 * Assume page size of 4096 bytes.
 */
#define STRATUM_PAGE_SIZE          4096

/// Size of each individual arena (256 KiB)
#define STRATUM_ARENA_SIZE         (256u << 10u)
constexpr auto kStratumPoolsAvailable = STRATUM_ARENA_SIZE / STRATUM_PAGE_SIZE;

/**
 * @brief Memory quantum.
 *
 * The allocated memory is always aligned to this value.
 */
#define STRATUM_QUANTUM            8

/// Maximum size of blocks managed by the memory pool.
#define STRATUM_BLOCK_MAX_SIZE     1024
constexpr auto kStratumClasses = STRATUM_BLOCK_MAX_SIZE / STRATUM_QUANTUM;

/*
 * Stratum memory layout:
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
 *     ^       ^       ^       ^
 *     +-------+-------+-------+--- < MEMORY PAGES (STRATUM_PAGE_SIZE)
 */

namespace stratum {
    struct alignas(STRATUM_QUANTUM)Arena {
        /// Total pools in the arena.
        unsigned int pools;

        /// Number of free pools in the arena.
        unsigned int free;

        /// Pointer to linked-list of available pools.
        struct Pool *pool;

        /// Pointers to next arena.
        struct Arena *next;

        /// Pointers to prev arena.
        struct Arena **prev;
    };

    struct alignas(STRATUM_QUANTUM)Pool {
        /// Pointer to Arena.
        Arena *arena;

        /// Total blocks in pool.
        unsigned short blocks;

        /// Free blocks in pool.
        unsigned short free;

        /// Size of single memory block.
        unsigned short blocksz;

        /// Pointer to linked-list of available blocks.
        void *block;

        /// Pointers to next pool of a specific size-class.
        struct Pool *next;

        /// Pointers to prev pool of a specific size-class.
        struct Pool **prev;
    };

    inline void *AlignDown(const void *ptr, const size_t sz) {
        return (void *) (((uintptr_t) ptr) & ~(sz - 1));
    }

    inline void *AlignUp(const void *ptr, const size_t sz) {
        return (void *) (((uintptr_t) ptr + sz) & ~(sz - 1));
    }

    inline size_t SizeToPoolClass(size_t size) {
        return (((size + (STRATUM_QUANTUM - 1)) & ~((size_t) STRATUM_QUANTUM - 1)) / STRATUM_QUANTUM) - 1;
    }

    inline size_t ClassToSize(size_t clazz) {
        return STRATUM_QUANTUM + (STRATUM_QUANTUM * clazz);
    }

    inline bool AddressInArenas(const void *ptr) {
        const auto *p = (Pool *) AlignDown(ptr, STRATUM_PAGE_SIZE);
        return p->arena != nullptr
               && (((uintptr_t) ptr - (uintptr_t) AlignDown(p->arena, STRATUM_PAGE_SIZE)) < STRATUM_ARENA_SIZE)
               && p->arena->pools == kStratumPoolsAvailable;
    }

    Arena *AllocArena();

    Pool *AllocPool(Arena *arena, size_t clazz);

    void FreeArena(Arena *arena);

    void FreePool(Pool *pool);

    void *AllocBlock(Pool *pool);

    void FreeBlock(Pool *pool, void *block);

} // namespace stratum

#endif // !STRATUM_ARENA_H_
