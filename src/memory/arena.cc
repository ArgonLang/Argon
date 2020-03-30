// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arena.h"
#include "osmemory.h"

#include <cstddef>
#include <cassert>
#include <new>

using namespace argon::memory;

Arena *argon::memory::AllocArena() {
    void *mem = argon::memory::os::Alloc(ARGON_MEMORY_ARENA_SIZE);
    Arena *arena;

    // Arena was located in the last bytes of the first Pool
    arena = new(((unsigned char *) mem) + (ARGON_MEMORY_PAGE_SIZE - sizeof(Arena))) Arena();

    arena->pools = ARGON_MEMORY_POOLS_AVAILABLE;
    arena->free = arena->pools;

    // Setup first Pool at the beginning of the allocated memory
    arena->pool = new(mem)Pool();
    arena->pool->arena = arena;

    return arena;
}

void argon::memory::FreeArena(Arena *arena) {
    void *mem = AlignDown(arena, ARGON_MEMORY_PAGE_SIZE);
    argon::memory::os::Free(mem, ARGON_MEMORY_ARENA_SIZE);
}

Pool *argon::memory::AllocPool(Arena *arena, size_t clazz) {
    size_t bytes = ARGON_MEMORY_PAGE_SIZE - sizeof(Pool);
    Pool *pool = arena->pool;

    assert(pool != nullptr);

    arena->free--;

    arena->pool = pool->next;
    if (arena->pool == nullptr && arena->free > 0) {
        arena->pool = new(AlignUp(pool, ARGON_MEMORY_PAGE_SIZE))Pool();
        arena->pool->arena = arena;
    }

    if (pool == AlignDown(arena, ARGON_MEMORY_PAGE_SIZE))
        bytes -= sizeof(Arena);

    pool->next = nullptr;
    pool->prev = nullptr;
    pool->blocksz = (unsigned short) ClassToSize(clazz);
    pool->blocks = (unsigned short) bytes / pool->blocksz;
    pool->free = pool->blocks;
    pool->block = ((unsigned char *) pool) + sizeof(Pool);
    *((Uintptr *) pool->block) = 0x0;

    return pool;
}

void argon::memory::FreePool(Pool *pool) {
    Arena *arena = pool->arena;
    pool->next = arena->pool;
    arena->pool = pool;
    arena->free++;
    assert(arena->free <= arena->pools);
}

void *argon::memory::AllocBlock(Pool *pool) {
    void *block = pool->block;

    assert(block != nullptr);

    pool->free--;

    pool->block = (void *) *((Uintptr *) pool->block);
    if (pool->block == nullptr && pool->free > 0) {
        pool->block = ((unsigned char *) block) + pool->blocksz;
        *((Uintptr *) pool->block) = 0x0;
    }

    return block;
}

void argon::memory::FreeBlock(Pool *pool, void *block) {
    *((Uintptr *) block) = (Uintptr) pool->block;
    pool->block = block;
    pool->free++;
    assert(pool->free <= pool->blocks);
}