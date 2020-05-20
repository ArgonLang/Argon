// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_REFCOUNT_H_
#define ARGON_OBJECT_REFCOUNT_H_

#include <atomic>

#include <memory/memory.h>

/*
 *    +------------- Overflow flag
 *    |
 *    |              Inline flag -------+
 *    |                                 |
 *    |              Static flag ---+   |
 *    |                             |   |
 *    v                             v   v
 *    +-+-+-----------------------+-+-+-+-+
 *    |   | Strong inline counter |   |   |
 * +  +---+-----------------------+---+---+  +
 * |                                         |
 * +------------+ uintptr_t +----------------+
 */

namespace argon::object {

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

        static const unsigned char StrongShift = After(Static);
        static const unsigned char StrongBits = CounterBits(Static) - 1;
        static const uintptr_t StrongMask = Mask(Strong);

        static const unsigned char StrongVFLAGShift = After(Strong);
        static const unsigned char StrongVFLAGBits = 1;
        static const uintptr_t StrongVFLAGMask = Mask(StrongVFLAG);
    };

    struct SideTable {
        std::atomic_uintptr_t strong_;
        std::atomic_uintptr_t weak_;
        // TODO: ArObject
    };

#define ARGON_OBJECT_REFCOUNT_INLINE 0x01
#define ARGON_OBJECT_REFCOUNT_STATIC 0x02

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

        [[nodiscard]] uintptr_t GetStrong() const {
            return GET_FIELD(Strong);
        }

        [[nodiscard]] SideTable *GetSideTable() const {
            return (SideTable *) this->bits_;
        }

        [[nodiscard]] bool IsInlineCounter() const {
            return GET_FIELD(Inline);
        }

        [[nodiscard]] bool IsStatic() const {
            return GET_FIELD(Static);
        }

#undef SET_FIELD
#undef GET_FIELD
    };

    class RefCount {
        std::atomic<RefBits> bits_{};
    public:
        RefCount() = default;

        explicit RefCount(uintptr_t status) {
            this->bits_.store(RefBits(status), std::memory_order_consume);
        }

        void IncStrong() {
            RefBits current = this->bits_.load(std::memory_order_consume);
            RefBits desired = {};

            if (current.IsStatic())
                return;

            do {
                desired = current;

                if (!desired.IsInlineCounter()) {
                    desired.GetSideTable()->strong_++;
                    return;
                }

                if (desired.Increment()) {
                    // Inline counter overflow
                    // TODO: ??
                    return;
                }
            } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_relaxed));
        }

        bool DecStrong() {
            RefBits current = this->bits_.load(std::memory_order_consume);
            RefBits desired = {};
            bool release;

            if (current.IsStatic())
                return false;

            do {
                desired = current;

                if (!desired.IsInlineCounter()) {
                    // TODO: ??
                    return false;
                }

                release = desired.Decrement();
            } while (!this->bits_.compare_exchange_weak(current, desired, std::memory_order_relaxed));

            return release;
        }
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_REFCOUNT_H_
