// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <stratum/arena.h>
#include <stratum/osmemory.h>
#include <stratum/memutil.h>

using namespace stratum;

Arena *stratum::AllocArena() {
    Arena *arena;
    void *mem;

    if ((mem = stratum::os::Alloc(STRATUM_ARENA_SIZE)) == nullptr)
        return nullptr;

    // Arena was located in the last bytes of the first Pool
    arena = (Arena *) (((unsigned char *) mem) + (STRATUM_PAGE_SIZE - sizeof(Arena)));
    arena->pools = kStratumPoolsAvailable;
    arena->free = kStratumPoolsAvailable;

    // Setup first Pool at the beginning of the allocated memory
    arena->pool = (Pool *) mem;

    util::MemoryZero(arena->pool, sizeof(Pool));

    arena->pool->arena = arena;

    arena->next = nullptr;
    arena->prev = nullptr;

    return arena;
}

void stratum::FreeArena(Arena *arena) {
    auto *mem = AlignDown(arena, STRATUM_PAGE_SIZE);
    stratum::os::Free(mem, STRATUM_ARENA_SIZE);
}

Pool *stratum::AllocPool(Arena *arena, size_t clazz) {
    size_t bytes = STRATUM_PAGE_SIZE - sizeof(Pool);
    auto *pool = arena->pool;

    assert(pool != nullptr);

    arena->free--;

    arena->pool = pool->next; // arena->pool = arena->pool->next
    if (arena->pool == nullptr && arena->free > 0) {
        auto new_pool = (Pool *) AlignUp(pool, STRATUM_PAGE_SIZE);
        new_pool->arena = arena;
        new_pool->blocks = 0;
        new_pool->free = 0;
        new_pool->blocksz = 0;
        new_pool->block = nullptr;
        new_pool->next = nullptr;
        new_pool->prev = nullptr;

        arena->pool = new_pool;
    }

    if (pool == AlignDown(arena, STRATUM_PAGE_SIZE))
        bytes -= sizeof(Arena);

    pool->blocksz = (unsigned short) ClassToSize(clazz);
    pool->blocks = (unsigned short) bytes / pool->blocksz;

    pool->free = pool->blocks;
    pool->block = ((unsigned char *) pool) + sizeof(Pool);
    *((uintptr_t *) pool->block) = 0x0;

    pool->next = nullptr;
    pool->prev = nullptr;

    return pool;
}

void stratum::FreePool(Pool *pool) {
    auto *arena = pool->arena;
    pool->next = arena->pool;
    arena->pool = pool;
    arena->free++;
    assert(arena->free <= arena->pools);
}

void *stratum::AllocBlock(Pool *pool) {
    void *block = pool->block;

    assert(block != nullptr);

    pool->free--;

    pool->block = (void *) *((uintptr_t *) pool->block);
    if (pool->block == nullptr && pool->free > 0) {
        pool->block = ((unsigned char *) block) + pool->blocksz;
        *((uintptr_t *) pool->block) = 0x0;
    }

    return block;
}

void stratum::FreeBlock(Pool *pool, void *block) {
    *((uintptr_t *) block) = (uintptr_t) pool->block;
    pool->block = block;
    pool->free++;
    assert(pool->free <= pool->blocks);
}