// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MEMORY_ARENA_H_
#define ARGON_MEMORY_ARENA_H_

#include <cstdint>
#include <mutex>

#define ARGON_MEMORY_PAGE_SIZE          4096                                        // Assume page size of 4096 bytes
#define ARGON_MEMORY_ARENA_SIZE         (((unsigned int)256) << (unsigned int)10)   // 256 KiB

#define ARGON_MEMORY_QUANTUM            8
#define ARGON_MEMORY_BLOCK_MAX_SIZE     512
#define ARGON_MEMORY_CLASSES            (ARGON_MEMORY_BLOCK_MAX_SIZE / ARGON_MEMORY_QUANTUM)

namespace argon::memory {
    using Uintptr = uintptr_t;

    inline void *AlignDown(const void *ptr, const size_t sz) {
        return (void *) (((Uintptr) ptr) & ~(sz - 1));
    }

    inline void *AlignUp(const void *ptr, const size_t sz) {
        return (void *) (((Uintptr) ptr + sz) & ~(sz - 1));
    }

    inline size_t SizeToPoolClass(size_t size) {
        return (((size + ARGON_MEMORY_QUANTUM) & ~((size_t) ARGON_MEMORY_QUANTUM - 1)) / ARGON_MEMORY_QUANTUM) - 1;
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

    template<typename T>
    class MemoryObject {
        void Insert(T *obj, T *pos) {
            obj->prev = pos;
            obj->next = pos->next;
            pos->next = obj;
            if (obj->next != nullptr)
                obj->next->prev = obj;
            else
                tail = obj;
        }

    public:
        std::mutex lock;
        T *head = nullptr;
        T *tail = nullptr;

        void Append(T *obj) {
            if (this->head == nullptr) {
                head = obj;
                tail = obj;
                return;
            }

            obj->prev = tail;
            tail->next = obj;
            tail = obj;
        }

        void Remove(T *obj) {
            if (obj->prev != nullptr)
                obj->prev->next = obj->next;
            else
                head = obj->next;

            if (obj->next != nullptr)
                obj->next->prev = obj->prev;
            else
                tail = obj->prev;
        }

        void Sort(T *obj) {
            T *cand = obj;

            for (T *cur = obj; cur != nullptr && obj.free >= cur.free; cand = cur, cur = cur->next);

            if (cand != obj) {
                Remove(obj);
                Insert(obj, cand);
            }
        }
    };

} // namespace argon::memory

#endif // !ARGON_MEMORY_ARENA_H_
