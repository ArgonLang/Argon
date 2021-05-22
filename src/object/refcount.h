// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_REFCOUNT_H_
#define ARGON_OBJECT_REFCOUNT_H_

#include <cassert>
#include <atomic>

#include <memory/memory.h>

#include "bitoffset.h"

/*
 *      +----------- Overflow flag
 *      |
 *      |                Inline flag -------+
 *      |                                   |
 *      |                Static flag ---+   |
 *      |                               |   |
 *      |                 GC flag --+   |   |
 *      |                           |   |   |
 *      v                           v   v   v
 *    +-+-+-----------------------+-+-+-+-+-+-+
 *    |   | Strong inline counter |   |   |   |
 * +  +---+-----------------------+-+-+---+---+  +
 * |                                             |
 * +----------------+ uintptr_t +----------------+
 */

namespace argon::object {
    struct SideTable {
        std::atomic_uintptr_t strong;
        std::atomic_uintptr_t weak;

        struct ArObject *object;
    };

    class RefBits {
        uintptr_t bits_;

#define SET_FIELD(name, value)  (this->bits_ = (this->bits_ & ~BitOffsets::name##Mask) | \
                                ((uintptr_t(value) << BitOffsets::name##Shift) & BitOffsets::name##Mask))
#define GET_FIELD(name)         ((this->bits_ & RCBitOffsets::name##Mask) >> RCBitOffsets::name##Shift)

    public:
        RefBits() = default;

        explicit RefBits(uintptr_t bits) {
            this->bits_ = bits;
        }

        bool Increment() {
            this->bits_ += uintptr_t(1) << RCBitOffsets::StrongShift;
            return this->bits_ & RCBitOffsets::StrongVFLAGMask;
        }

        bool Decrement() {
            this->bits_ -= uintptr_t(1) << RCBitOffsets::StrongShift;
            return (this->bits_ & RCBitOffsets::StrongMask) == 0;
        }

        void SetGCBit() {
            this->bits_ |= RCBitOffsets::GCMask;
        }

        [[nodiscard]] uintptr_t GetStrong() const {
            return GET_FIELD(Strong);
        }

        [[nodiscard]] SideTable *GetSideTable() const {
            return (SideTable *) (this->bits_ & ~RCBitOffsets::GCMask);
        }

        [[nodiscard]] bool IsInlineCounter() const {
            return GET_FIELD(Inline);
        }

        [[nodiscard]] bool IsStatic() const {
            return GET_FIELD(Static);
        }

        [[nodiscard]] bool IsGcObject() const {
            return GET_FIELD(GC);
        }

#undef SET_FIELD
#undef GET_FIELD
    };

    enum class RCType : unsigned char {
        INLINE = (unsigned char) 0x08 | (unsigned char) 0x01,
        STATIC = 0x02,
        GC = 0x08 | ((unsigned char) 0x04 | (unsigned char) 0x01)
    };

    class RefCount {
        std::atomic<RefBits> bits_{};

        SideTable *AllocOrGetSideTable();

        struct ArObject *GetObjectBase();

    public:
        RefCount() = default;

        explicit RefCount(RefBits status) noexcept;

        explicit RefCount(RCType status) noexcept;

        RefCount &operator=(RefBits status);

        struct ArObject *GetObject();

        RefBits IncWeak();

        bool DecStrong();

        bool DecWeak();

        bool IsGcObject();

        uintptr_t GetStrongCount();

        uintptr_t GetWeakCount();

        void ClearWeakRef();

        void IncStrong();
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_REFCOUNT_H_
