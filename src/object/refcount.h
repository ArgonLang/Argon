// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_REFCOUNT_H_
#define ARGON_OBJECT_REFCOUNT_H_

#include <cassert>
#include <atomic>

#include <memory/memory.h>
#include <memory/arena.h>

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

#if (ARGON_MEMORY_QUANTUM % 8)
#error RefCount uses a tagged pointer to optimally manage references to an object and needs at least 3 (less significant) bits free
#endif

    struct BitOffsets {
#define Mask(name)          (((uintptr_t(1)<<name##Bits)-1) << name##Shift)
#define After(name)         (name##Shift + name##Bits)
#define CounterBits(name)   (sizeof(uintptr_t) * 8) - After(name)

        static const unsigned char InlineShift = 0;
        static const unsigned char InlineBits = 1;
        static const uintptr_t InlineMask = Mask(Inline);

        static const unsigned char StaticShift = After(Inline);
        static const unsigned char StaticBits = 1;
        static const uintptr_t StaticMask = Mask(Static);

        static const unsigned char GCShift = After(Static);
        static const unsigned char GCBits = 1;
        static const uintptr_t GCMask = Mask(GC);

        static const unsigned char StrongShift = After(GC);
        static const unsigned char StrongBits = CounterBits(Static) - 2;
        static const uintptr_t StrongMask = Mask(Strong);

        static const unsigned char StrongVFLAGShift = After(Strong);
        static const unsigned char StrongVFLAGBits = 1;
        static const uintptr_t StrongVFLAGMask = Mask(StrongVFLAG);
    };

    struct SideTable {
        std::atomic_uintptr_t strong;
        std::atomic_uintptr_t weak;

        class ArObject *object;
    };

    class RefBits {
        uintptr_t bits_;

#define SET_FIELD(name, value)  (this->bits_ = (this->bits_ & ~BitOffsets::name##Mask) | \
                                ((uintptr_t(value) << BitOffsets::name##Shift) & BitOffsets::name##Mask))
#define GET_FIELD(name)         ((this->bits_ & BitOffsets::name##Mask) >> BitOffsets::name##Shift)

    public:
        RefBits() = default;

        explicit RefBits(uintptr_t bits) {
            this->bits_ = bits;
        }

        bool Increment() {
            this->bits_ += uintptr_t(1) << BitOffsets::StrongShift;
            return this->bits_ & BitOffsets::StrongVFLAGMask;
        }

        bool Decrement() {
            this->bits_ -= uintptr_t(1) << BitOffsets::StrongShift;
            return (this->bits_ & BitOffsets::StrongMask) == 0;
        }

        void SetGCBit() {
            this->bits_ |= BitOffsets::GCMask;
        }

        [[nodiscard]] uintptr_t GetStrong() const {
            return GET_FIELD(Strong);
        }

        [[nodiscard]] SideTable *GetSideTable() const {
            return (SideTable *) (this->bits_ & ~BitOffsets::GCMask);
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
        GC = 0x0D
    };

    class RefCount {
        std::atomic<RefBits> bits_{};

        SideTable *AllocOrGetSideTable();

        class ArObject *GetObjectBase();

    public:
        RefCount() = default;

        explicit RefCount(RefBits status);

        explicit RefCount(RCType status);

        RefCount &operator=(RefBits status);

        void IncStrong();

        RefBits IncWeak();

        bool DecStrong();

        bool DecWeak();

        bool IsGcObject();

        uintptr_t GetStrongCount();

        class ArObject *GetObject();
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_REFCOUNT_H_
