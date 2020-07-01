// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_UTILS_ENUMBITMASK_H_
#define ARGON_UTILS_ENUMBITMASK_H_

template<typename Enum>
struct EnableBitMaskOps {
    static const bool enable = false;
};

#define ENUMBITMASK_ISTRUE(l, r)    (l & r) == r
#define ENUMBITMASK_ISFALSE(l, r)   (l & r) != r
#define ENUMBITMASK_ENABLE(x)       \
template<>                          \
struct EnableBitMaskOps<x> {static const bool enable = true;}

#define _ENUMBITMASK_ASSERT         static_assert(std::is_enum<Enum>::value, "template parameter must be an enum class")
#define _ENUMBITMASK_UNDERLYING     using underlying = typename std::underlying_type<Enum>::type

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type operator~(Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    return static_cast<Enum>(~static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type operator&(Enum lhs, Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type operator^(Enum lhs, Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type operator|(Enum lhs, Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type &operator&=(Enum &lhs, Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    lhs = static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type &operator^=(Enum &lhs, Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    lhs = static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
    return lhs;
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOps<Enum>::enable, Enum>::type &operator|=(Enum &lhs, Enum rhs) {
    _ENUMBITMASK_ASSERT;
    _ENUMBITMASK_UNDERLYING;
    lhs = static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

#endif // !ARGON_UTILS_ENUMBITMASK_H_
