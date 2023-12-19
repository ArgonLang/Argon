// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_MEMORY_REFCOUNT_H_
#define ARGON_VM_MEMORY_REFCOUNT_H_

#include <atomic>

#include <argon/vm/memory/bitoffset.h>

/*
 *      +----------- Overflow flag
 *      |
 *      |                Inline flag -------+
 *      |                                   |
 *      |                    GC flag ---+   |
 *      |                               |   |
 *      |             Static flag --+   |   |
 *      |                           |   |   |
 *      v                           v   v   v
 *    +-+-+-----------------------+-+-+-+-+-+-+
 *    |   | Strong inline counter |   |   |   |
 * +  +---+-----------------------+-+-+---+---+  +
 * |                                             |
 * +----------------+ uintptr_t +----------------+
 */

namespace argon::vm::memory {
    using RCObject = struct ArObject *;

    enum class RCType {
        INLINE = 0x08u | 0x01,
        STATIC = 0x04,
        GC = 0x08u | (0x02u | 0x01)
    };

    /**
     * @brief This structure represents a SideTable.
     *
     * It is used to store weak references to an object, or in case of an inline counter overflow.
     */
    struct SideTable {
        /// Strong references counter.
        std::atomic_uintptr_t strong;

        /// Weak references counter.
        std::atomic_uintptr_t weak;

        /// Object pointer.
        RCObject object;
    };

    /**
     * @brief Manipulate the counter bits.
     */
    class RefBits {
        uintptr_t bits_;

#define SET_FIELD(name, value)  (this->bits_ = (this->bits_ & ~BitOffsets::name##Mask) | \
                                ((uintptr_t(value) << BitOffsets::name##Shift) & BitOffsets::name##Mask))
#define GET_FIELD(name)         ((this->bits_ & RCBitOffsets::name##Mask) >> RCBitOffsets::name##Shift)

    public:
        RefBits() = default;

        explicit constexpr RefBits(uintptr_t bits) : bits_(bits) {}

        /**
         * @brief Increment strong references counter.
         * @return Returns true on overflow, otherwise false.
         */
        bool Increment() {
            this->bits_ += uintptr_t(1) << RCBitOffsets::StrongShift;
            return this->bits_ & RCBitOffsets::StrongVFLAGMask;
        }

        /**
         * @brief Decrement strong references counter.
         * @return Returns true if there are no more strong references to the object, otherwise false.
         */
        bool Decrement() {
            this->bits_ -= uintptr_t(1) << RCBitOffsets::StrongShift;
            return (this->bits_ & RCBitOffsets::StrongMask) == 0;
        }

        /**
         * @brief Set the GC bit.
         */
        void SetGCBit() {
            this->bits_ |= RCBitOffsets::GCMask;
        }

        /**
         * @brief Get strong references count.
         * @return Strong references count.
         */
        [[nodiscard]] uintptr_t GetStrong() const {
            return GET_FIELD(Strong);
        }

        /**
         * @brief Get the pointer to the SideTable.
         * @return Pointer to the SideTable.
         */
        [[nodiscard]] SideTable *GetSideTable() const {
            return (SideTable *) (this->bits_ & ~RCBitOffsets::GCMask);
        }

        /**
         * @brief Check if the associated object has an inline counter.
         * @return True if the object has an inline counter, false otherwise.
         */
        [[nodiscard]] bool IsInlineCounter() const {
            return GET_FIELD(Inline);
        }

        /**
         * @brief Check if the associated object is immortal.
         * @return True if the object is immortal, false otherwise.
         */
        [[nodiscard]] bool IsStatic() const {
            return GET_FIELD(Static);
        }

        /**
         * @brief Check if the object is managed by the GC.
         * @return True if the object is managed by the GC, false otherwise.
         */
        [[nodiscard]] bool IsGcObject() const {
            return GET_FIELD(GC);
        }

#undef SET_FIELD
#undef GET_FIELD
    };

    /**
     * @brief This class represents the reference counter
     */
    class RefCount {
        std::atomic<RefBits> bits_{};

        SideTable *AllocOrGetSideTable();

        RCObject GetObjectBase();

    public:
        explicit constexpr RefCount(RCType status) noexcept: bits_(RefBits((unsigned char) status)) {};

        RefCount &operator=(RCType type) {
            this->bits_ = RefBits((uintptr_t) type);
            return *this;
        }

        RefCount &operator=(RefBits bits) {
            this->bits_ = bits;
            return *this;
        }

        /**
         * @brief Release a strong reference.
         * @return True if the object no longer has strong references, false otherwise.
         */
        bool DecStrong(RefBits *out);

        /**
         * @brief Release a weak reference.
         * @return True if the object no longer has weak references, false otherwise.
         */
        bool DecWeak() const;

        /**
         * @brief Check if the object has the SideTable.
         * @return True if the object has the SideTable, false otherwise.
         */
        bool HaveSideTable() const;

        /**
         * @brief Increment strong references counter.
         *
         * The only reason this function returns false is if the counter overflows
         * and it is not possible to create a SideTable due to lack of memory.
         *
         * @return True if the strong reference counter has been increased, false otherwise.
         */
        bool IncStrong();

        /**
         * @brief Check if the object is managed by the GC.
         * @return True if the object is managed by the GC, false otherwise.
         */
        bool IsGcObject() const {
            return this->bits_.load(std::memory_order_seq_cst).IsGcObject();
        }

        /**
         * @brief Check if the associated object is immortal.
         * @return True if the object is immortal, false otherwise.
         */
        bool IsStatic() const {
            return this->bits_.load(std::memory_order_seq_cst).IsStatic();
        }

        /**
         * @brief Returns the object it is associated with.
         *
         * @return Returns the object it is associated with. In case of weak reference,
         * it can return the object or nullptr if the object has already been deleted.
         */
        RCObject GetObject();

        /**
         * @brief Increase the number of weak references.
         *
         * If the object does not have a SideTable then it will be created and if successful the weak reference
         * will be incremented and an integer representing the pointer to the SideTable will be returned,
         * otherwise 0 will be returned.
         *
         * @return An integer representing the pointer to SideTable in case of success, 0 otherwise.
         */
        RefBits IncWeak();

        /**
         * @brief Get strong references count.
         * @return Strong references count.
         */
        uintptr_t GetStrongCount() const;

        /**
         * @brief Get weak references count.
         * @return weak references count.
         */
        uintptr_t GetWeakCount() const;
    };
} // namespace argon::vm::memory

#endif // !ARGON_VM_MEMORY_REFCOUNT_H_
