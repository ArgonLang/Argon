// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0
// EasterEgg: \qpu ;)

#ifndef STRATUM_MEMORY_H_
#define STRATUM_MEMORY_H_

#include <cstddef>
#include <new>

#include <support/linklist.h>

#include "arena.h"

/* Minimum number of arenas, Stratum WILL NEVER release this memory to the OS. */
#define STRATUM_MINIMUM_POOL       16

#define STRATUM_REALLOC_THRESHOLD  10

namespace stratum {
    class Memory {
        /* Arena Linked-List */
        support::LinkedList<Arena> arenas_;
        std::mutex m_arenas_;

        /* Memory pools organized by size-class */
        support::LinkedList<Pool> pools_[kStratumClasses];
        std::mutex m_pools_[kStratumClasses];

        Arena *FindOrCreateArena();

        Pool *AllocatePool(size_t clazz);

        Pool *GetPool(size_t clazz);

        void TryReleaseMemory(Pool *pool, size_t clazz);

    public:
        /**
         * @brief Initialize memory manager.
         *
         * This call is not strictly necessary but is strongly recommended before starting to use the memory manager.
         * Once invoked it tries to allocate a pool equal to STRATUM_MINIMUM_POOL.
         *
         * @return true in case of success, false otherwise.
         */
        bool Initialize();

        /**
         * @brief Allocates a block of size bytes of memory, returning a pointer to the beginning of the block.
         *
         * Allocates a block of memory aligned to the value of STRATUM_QUANTUM.
         * If the requested size is greater than STRATUM_BLOCK_MAX_SIZE the memory is allocated by calling
         * standard malloc and aligning the returned value to STRATUM_QUANTUM.
         *
         * @param size Memory size required.
         * @return A void * to the beginning of the allocated block.
         */
        void *Alloc(size_t size);

        /**
         * @brief Allocates a block of memory and instantiate an object of type \p T.
         *
         * @tparam T Class to instantiate.
         * @tparam Args Class constructor arguments.
         * @return An object of type \p T.
         */
        template<typename T, typename ...Args>
        inline T *AllocObject(Args ...args) {
            auto mem = Alloc(sizeof(T));
            T *obj = nullptr;

            if (mem != nullptr) {
                try {
                    obj = new(mem) T(args...);
                } catch (...) {
                    Free(mem);
                    throw;
                }
            }

            return obj;
        }

        /**
         * @brief Allocate and zero-initialize array.
         *
         * Allocates a block of memory for an array of \p num elements, each of them \p size bytes long,
         * and initializes all its bits to zero.
         *
         * @param num Number of elements to allocate.
         * @param size Size of each element.
         * @return A void * to the beginning of the allocated block.
         */
        void *Calloc(size_t num, size_t size);

        /**
         * @brief Allocate and zero-initialize array containing elements of size 1.
         *
         * Allocates a block of memory for an array of \p num elements, each of them 1 bytes long,
         * and initializes all its bits to zero.
         *
         * @param num Number of elements to allocate.
         * @return A void * to the beginning of the allocated block.
         */
        inline void *Calloc(size_t num) {
            return this->Calloc(num, 1);
        }

        /**
         * @brief Release all memory managed by this instance of Memory.
         */
        void Finalize();

        /**
         * @brief Release a block of memory previously allocated by a call to Memory::Alloc, Memory::Calloc
         * or Memory::Realloc.
         *
         * Releases the previously allocated block of memory making it available again for further allocation.
         * If the value of the pointer passed is nullptr, the call returns without doing anything.
         *
         * @param ptr Pointer to memory block to be freed.
         */
        void Free(void *ptr);

        /**
         * @brief Call the object destructor and release the used memory block.
         *
         * @param obj Pointer to object to be freed.
         */
        template<typename T>
        inline void FreeObject(T *obj) {
            obj->~T();
            Free(obj);
        }

        /**
         * @brief Changes the size of the memory block pointed to by ptr.
         *
         * The function may move the memory block to a new location (whose address is returned by the function).
         * In case that ptr is a nullptr, the function behaves like Memory::Alloc, assigning a new block of size bytes
         * and returning a pointer to its beginning.
         *
         * @param ptr Pointer to the previously allocated memory block or nullptr.
         * @param size New memory size required.
         * @return A void * to the beginning of the allocated block.
         */
        void *Realloc(void *ptr, size_t size);
    };

    extern Memory default_allocator;

    /**
     * @brief Like Memory::Alloc but on the default instance.
     *
     * @param size Memory size required.
     * @return A void * to the beginning of the allocated block.
     */
    void *Alloc(size_t size);

    /**
     * @brief Like Memory::AllocObject but on the default instance.
     *
     * @tparam T Class to instantiate.
     * @tparam Args Class constructor arguments.
     * @return An object of type \p T.
     */
    template<typename T, typename ...Args>
    inline T *AllocObject(Args ...args) {
        return default_allocator.AllocObject<T>(args...);
    }

    /**
     * @brief Like Memory::Calloc but on the default instance.
     *
     * @param num Number of elements to allocate.
     * @param size Size of each element.
     * @return A void * to the beginning of the allocated block.
     */
    void *Calloc(size_t num, size_t size);

    /**
     * @brief Like Memory::Calloc but on the default instance.
     *
     * @param num Number of elements to allocate.
     * @return A void * to the beginning of the allocated block.
     */
    inline void *Calloc(size_t num) {
        return Calloc(num, 1);
    }

    /**
     * @brief Initialize default instance of memory manager.
     *
     * @return true in case of success, false otherwise.
     */
    bool Initialize();

    /**
     * @brief Release all memory managed by default instance of the memory manager.
     */
    void Finalize();

    /**
     * @brief Like Memory::Free but on the default instance.
     *
     * @param ptr Pointer to memory block to be freed.
     */
    void Free(void *ptr);

    /**
     * @brief Like Memory::FreeObject but on the default instance.
     *
     * @param obj Pointer to object to be freed.
     */
    template<typename T>
    inline void FreeObject(T *obj) {
        obj->~T();
        default_allocator.Free(obj);
    }

    /**
     * @brief Like Memory::Realloc but on the default instance.
     *
     * @param ptr Pointer to the previously allocated memory block or nullptr.
     * @param size New memory size required.
     * @return A void * to the beginning of the allocated block.
     */
    void *Realloc(void *ptr, size_t size);
} // namespace stratum

#endif // !STRATUM_MEMORY_H_
