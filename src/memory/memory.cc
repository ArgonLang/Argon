// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "memory.h"
#include "memobj.h"
#include "arena.h"

#include <cassert>

using namespace argon::memory;

/* Arena Linked-List */
MemoryObject<Arena> arenas;

/* Memory pools organized by size-class */
MemoryObject<Pool> pools[ARGON_MEMORY_CLASSES];

Arena *FindOrCreateArena() {
    Arena *arena = arenas.FindFree();

    if (arena == nullptr) {
        arena = AllocArena();
        arenas.Append(arena);
    }

    return arena;
}

Pool *GetPool(size_t clazz) {
    Arena *arena = nullptr;
    Pool *pool = pools[clazz].FindFree();

    if (pool == nullptr) {
        arenas.lock.lock();
        arena = FindOrCreateArena();
        pool = AllocPool(arena, clazz);
        arenas.lock.unlock();
        pools[clazz].Append(pool);
    }

    return pool;
}

void TryReleaseMemory(Pool *pool, size_t clazz) {
    Arena *arena = pool->arena;

    if (pool->free == pool->blocks) {
        arenas.lock.lock();
        FreePool(pool);
        if (arena->free != arena->pools)
            arenas.Sort(arena);
        else if (arenas.count > ARGON_MEMORY_MINIMUM_POOL) {
            arenas.Remove(arena);
            FreeArena(arena);
        }
        arenas.lock.unlock();
    } else
        pools[clazz].Sort(pool);
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

void argon::memory::Free(void *ptr) {
    Pool *pool = (Pool *) AlignDown(ptr, ARGON_MEMORY_PAGE_SIZE);
    size_t clazz = SizeToPoolClass(pool->blocksz);

    pools[clazz].lock.lock();
    FreeBlock(pool, ptr);
    TryReleaseMemory(pool, clazz);
    pools[clazz].lock.unlock();
}

void argon::memory::InitializeMemory() {
    for (int i = 0; i < ARGON_MEMORY_MINIMUM_POOL; arenas.Append(AllocArena()), i++);
}

void argon::memory::FinalizeMemory() {
    Arena *arena = nullptr;
    Arena *next = nullptr;

    for (arena = arenas.head; arena != nullptr; arena = next) {
        next = arena->next;
        FreeArena(arena);
    }
}