// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_HASH_MAGIC_H_
#define ARGON_OBJECT_HASH_MAGIC_H_

#include <cstddef>

#include <utils/macros.h>
#include <object/arobject.h>

#if _ARGON_ENVIRON == 32
#define ARGON_OBJECT_HASH_BITS  31
#define ARGON_OBJECT_HASH_PRIME 2147483647 // pow(2,31) - 1
#else
#define ARGON_OBJECT_HASH_BITS  61
#define ARGON_OBJECT_HASH_PRIME 2305843009213693951 // pow(2,61) - 1
#endif

#define ARGON_OBJECT_HASH_NAN   0x0
#define ARGON_OBJECT_HASH_INF   0x4CB2F

namespace argon::object {
    ArSize HashBytes(const unsigned char *bytes, ArSize size);
}

#endif // !ARGON_OBJECT_HASH_MAGIC_H_
