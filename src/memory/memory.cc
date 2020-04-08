// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "memory.h"
#include "memobj.h"
#include "arena.h"

using namespace argon::memory;

inline void *GetRealMemoryPtr(void *ptr) { return ((unsigned char *) ptr) - sizeof(size_t); }

inline size_t GetEmbeddedSize(void *ptr) { return *((size_t *) (((unsigned char *) ptr) - sizeof(size_t))); }

inline void *SetEmbeddedSize(void *ptr, size_t size) {
    *((size_t *) ptr) = size;
    return ((unsigned char *) ptr) + sizeof(size_t);
}

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
        pools[clazz].Remove(pool);
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

void *argon::memory::Alloc(size_t size) noexcept {
    size_t clazz = SizeToPoolClass(size);
    Pool *pool = nullptr;
    void *mem = nullptr;

    if (size <= ARGON_MEMORY_BLOCK_MAX_SIZE) {
        pools[clazz].lock.lock();
        pool = GetPool(clazz);
        mem = AllocBlock(pool);
        pools[clazz].lock.unlock();
    } else {
        try {
            mem = operator new(size + sizeof(size_t));
            mem = SetEmbeddedSize(mem, size);
        } catch (std::bad_alloc &) {
            return nullptr;
        }
    }

    return mem;
}

void argon::memory::Free(void *ptr) {
    Pool *pool = (Pool *) AlignDown(ptr, ARGON_MEMORY_PAGE_SIZE);

    if (AddressInArenas(ptr)) {
        size_t clazz = SizeToPoolClass(pool->blocksz);
        pools[clazz].lock.lock();
        FreeBlock(pool, ptr);
        TryReleaseMemory(pool, clazz);
        pools[clazz].lock.unlock();
    } else ::operator delete(GetRealMemoryPtr(ptr));
}

void *argon::memory::MemoryCopy(void *dest, const void *src, size_t size) {
    auto d = (unsigned char *) dest;
    auto s = (const unsigned char *) src;

    while (size--)
        *d++ = *s++;

    return dest;
}

void *argon::memory::Realloc(void *ptr, size_t size) {
    Pool *pool = (Pool *) AlignDown(ptr, ARGON_MEMORY_PAGE_SIZE);
    bool in_arenas = AddressInArenas(ptr);
    size_t src_sz = 0;
    void *tmp = nullptr;

    if (in_arenas) {
        if (SizeToPoolClass(pool->blocksz) >= SizeToPoolClass(size)) return ptr;
        src_sz = pool->blocksz;
    } else {
        src_sz = GetEmbeddedSize(ptr);
        if (src_sz >= size) return ptr;
    }

    if ((tmp = Alloc(size)) == nullptr)
        return nullptr;

    MemoryCopy(tmp, ptr, src_sz);
    Free(ptr);
    return tmp;
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