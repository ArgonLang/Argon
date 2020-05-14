// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_REFCOUNT_H_
#define ARGON_OBJECT_REFCOUNT_H_

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
        static const unsigned char StrongBits = CounterBits(Static);
        static const uintptr_t StrongMask = Mask(Strong);
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_REFCOUNT_H_
