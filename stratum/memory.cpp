// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <cstdlib>

#include <stratum/memutil.h>
#include <stratum/memory.h>

#define POP_FROM_LIST(node)                 \
    do {                                    \
        if(node->next != nullptr)           \
            node->next->prev = node->prev;  \
                                            \
        *(node->prev) = node->next;         \
    } while(0)                              \

#define PUSH_TO_LIST(list, node)            \
    do{                                     \
        auto *current = list;               \
                                            \
        node->next = current;               \
        pool->prev = &list;                 \
                                            \
        if(current != nullptr)              \
            current->prev = &node->next;    \
                                            \
        list = node;                        \
    } while(0)

using namespace stratum;

struct Emb {
    size_t size;
    size_t offset;
};

Memory stratum::default_allocator;

Arena *Memory::FindOrCreateArena() {
    Arena *arena = this->arenas_.FindFree();

    if (arena == nullptr) {
        arena = AllocArena();
        this->arenas_.Insert(arena);
    }

    return arena;
}

bool Memory::Initialize() {
    std::unique_lock lck(this->m_arenas_);

    if (this->arenas_.Count() > 0)
        return true;

    for (int i = 0; i < STRATUM_MINIMUM_POOL; i++) {
        auto *arena = AllocArena();

        if (arena == nullptr) {
            if (i > 0)
                this->Finalize();

            return false;
        }

        this->arenas_.Insert(arena);
    }

    return true;
}

Pool *Memory::AllocatePool(size_t clazz) {
    this->m_arenas_.lock();

    auto *arena = this->FindOrCreateArena();
    if (arena == nullptr) {
        this->m_arenas_.unlock();

        return nullptr;
    }

    auto *pool = AllocPool(arena, clazz);

    this->m_arenas_.unlock();

    return pool;
}

Pool *Memory::GetPool(size_t clazz) {
    Pool *pool = this->pools_[clazz];

    if (pool == nullptr) {
        if ((pool = this->AllocatePool(clazz)) == nullptr)
            return nullptr;

        pool->prev = this->pools_ + clazz;
        this->pools_[clazz] = pool;
    }

    return pool;
}

void *Memory::AllocateBlockFromPool(size_t clazz) {
    auto *m_pool = this->m_pools_ + clazz;

    m_pool->lock();

    auto *pool = GetPool(clazz);
    if (pool == nullptr) {
        m_pool->unlock();

        return nullptr;
    }

    auto *block = AllocBlock(pool);

    if (pool->free == 0) {
        POP_FROM_LIST(pool);

        pool->next = nullptr;
        pool->prev = nullptr;
    }

    m_pool->unlock();

    return block;
}

void *Memory::Alloc(size_t size) {
    size_t clazz = SizeToPoolClass(size);

    assert(size > 0);

    if (size <= STRATUM_BLOCK_MAX_SIZE)
        return this->AllocateBlockFromPool(clazz);

    unsigned char *ptr;
    if ((ptr = (unsigned char *) malloc(size + sizeof(Emb) + STRATUM_QUANTUM)) == nullptr)
        return nullptr;

    auto emb_off = (uintptr_t) (ptr + sizeof(Emb));
    auto user_off = STRATUM_QUANTUM - (emb_off % STRATUM_QUANTUM);
    auto ptr_a = (unsigned char *) (emb_off + (user_off != STRATUM_QUANTUM ? user_off : 0));

    auto *emb = (Emb *) (ptr_a - sizeof(Emb));
    emb->size = size;
    emb->offset = ptr_a - ptr;

    return ptr_a;
}

void *Memory::Calloc(size_t num, size_t size) {
    void *area;

    if (num == 0 || size == 0)
        return nullptr;

    if ((area = this->Alloc(num * size)) == nullptr)
        return nullptr;

    util::MemoryZero(area, num * size);

    return area;
}

void Memory::Finalize() {
    Arena *arena;

    while ((arena = this->arenas_.Pop()) != nullptr)
        FreeArena(arena);
}

void Memory::Free(void *ptr) {
    if (ptr == nullptr)
        return;

    auto *pool = (Pool *) AlignDown(ptr, STRATUM_PAGE_SIZE);

    if (AddressInArenas(ptr)) {
        size_t clazz = SizeToPoolClass(pool->blocksz);
        auto *m_pool = this->m_pools_ + clazz;

        m_pool->lock();

        FreeBlock(pool, ptr);
        this->TryReleaseMemory(pool, clazz);

        m_pool->unlock();

        return;
    }

    const auto *emb = (Emb *) ((unsigned char *) ptr - sizeof(Emb));
    free(((unsigned char *) ptr) - emb->offset);
}

void *Memory::Realloc(void *ptr, size_t size) {
    void *tmp;
    size_t src_sz;

    if (ptr == nullptr)
        return this->Alloc(size);

    const auto *pool = (Pool *) AlignDown(ptr, STRATUM_PAGE_SIZE);

    if (AddressInArenas(ptr)) {
        size_t actual = SizeToPoolClass(pool->blocksz);
        size_t desired = SizeToPoolClass(size);
        src_sz = pool->blocksz;

        if (actual > desired && (actual - desired < STRATUM_REALLOC_THRESHOLD))
            return ptr;
    } else {
        const auto *emb = (Emb *) (((unsigned char *) ptr) - sizeof(Emb));

        src_sz = emb->size;

        if (size > STRATUM_BLOCK_MAX_SIZE && src_sz >= size)
            return ptr;
    }

    if ((tmp = Alloc(size)) == nullptr)
        return nullptr;

    util::MemoryCopy(tmp, ptr, src_sz > size ? size : src_sz);
    this->Free(ptr);
    return tmp;
}

void Memory::TryReleaseMemory(Pool *pool, size_t clazz) {
    Arena *arena = pool->arena;

    if (pool->free == pool->blocks) {
        this->m_arenas_.lock();

        if (pool->prev != nullptr)
            POP_FROM_LIST(pool);

        FreePool(pool);

        if (arena->free != arena->pools)
            this->arenas_.Sort(arena);
        else if (this->arenas_.Count() > STRATUM_MINIMUM_POOL) {
            this->arenas_.Remove(arena);
            FreeArena(arena);
        }

        this->m_arenas_.unlock();

        return;
    }

    if (pool->prev == nullptr)
        PUSH_TO_LIST(this->pools_[clazz], pool);
}

// DEFAULT ALLOCATOR

void *stratum::Alloc(size_t size) {
    return default_allocator.Alloc(size);
}

void *stratum::Calloc(size_t num, size_t size) {
    return default_allocator.Calloc(num, size);
}

void stratum::Free(void *ptr) {
    return default_allocator.Free(ptr);
}

void *stratum::Realloc(void *ptr, size_t size) {
    return default_allocator.Realloc(ptr, size);
}

bool stratum::Initialize() {
    return default_allocator.Initialize();
}

void stratum::Finalize() {
    default_allocator.Finalize();
}
