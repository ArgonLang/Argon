// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "memory.h"
#include "arena.h"
#include "osmemory.h"

#include <mutex>
#include <cassert>

using namespace argon::memory;

/* Arena LinkedList */
MemoryObject<Arena> arenas;

/* Memory pools organized by size-class */
MemoryObject<Pool> pools[ARGON_MEMORY_CLASSES];


Arena *AllocArena() {
    void *mem = argon::memory::os::Alloc(ARGON_MEMORY_ARENA_SIZE);
    Arena *arena;

    // Arena was located in the last bytes of the first Pool
    arena = new(((unsigned char *) mem) + (ARGON_MEMORY_PAGE_SIZE - sizeof(Arena))) Arena();

    arena->pools = ARGON_MEMORY_ARENA_SIZE / ARGON_MEMORY_PAGE_SIZE;
    arena->free = arena->pools;

    // Setup first Pool at the beginning of the allocated memory
    arena->pool = new(mem)Pool();
    arena->pool->arena = arena;

    return arena;
}

Pool *AllocPool(Arena *arena, size_t clazz) {
    size_t bytes = ARGON_MEMORY_PAGE_SIZE - sizeof(Pool);
    Pool *pool = arena->pool;

    assert(pool != nullptr);

    arena->pool = pool->next;
    if (arena->pool == nullptr) {
        void *nxtpool = AlignUp(pool, ARGON_MEMORY_PAGE_SIZE);
        if (nxtpool < ((unsigned char *) AlignDown(arena, ARGON_MEMORY_PAGE_SIZE)) + ARGON_MEMORY_ARENA_SIZE) {
            arena->pool = new(nxtpool)Pool();
            arena->pool->arena = arena;
        }
    }

    arena->free--;

    if (pool == AlignDown(arena, ARGON_MEMORY_PAGE_SIZE))
        bytes -= sizeof(Arena);

    pool->blocksz = ClassToSize(clazz);
    pool->blocks = bytes / pool->blocksz;
    pool->free = pool->blocks;
    pool->block = ((unsigned char *) pool) + sizeof(Pool);
    *((Uintptr *) pool->block) = 0x0;

    return pool;
}

void *AllocBlock(Pool *pool) {
    void *block = pool->block;

    assert(block != nullptr);

    pool->free--;

    pool->block = (void *) *((Uintptr *) pool->block);
    if (pool->block == nullptr && pool->free > 0) {
        pool->block = ((unsigned char *) block) + pool->blocksz;
    }

    return block;
}

Arena *FindOrCreateArena() {
    Arena *arena = nullptr;

    if (arenas.head != nullptr)
        for (arena = arenas.head; arena != nullptr && arena->free == 0; arena = arena->next);

    arena = AllocArena();
    arenas.Append(arena);

    return arena;
}

Pool *GetPool(size_t clazz) {
    Arena *arena = nullptr;
    Pool *pool = nullptr;

    for (pool = pools[clazz].head; pool != nullptr && pool->free == 0; pool = pool->next);

    if (pool == nullptr) {
        arenas.lock.lock();
        arena = FindOrCreateArena();
        pool = AllocPool(arena, clazz);
        arenas.lock.unlock();
        pools[clazz].Append(pool);
    }

    return pool;
}

void *argon::memory::Alloc(size_t size) {
    size_t clazz = SizeToPoolClass(size);
    Pool *pool = nullptr;
    void *mem = nullptr;

    pools[clazz].lock.lock();
    pool = GetPool(clazz);
    mem = AllocBlock(pool);
    pools[clazz].lock.unlock();

    return mem;
}