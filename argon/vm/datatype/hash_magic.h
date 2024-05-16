// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_HASH_MAGIC_H_
#define ARGON_VM_DATATYPE_HASH_MAGIC_H_

#include <cstddef>

#include <argon/util/macros.h>

#include <argon/vm/datatype/objectdef.h>

#if _ARGON_ENVIRON == 32
#define ARGON_OBJECT_HASH_BITS  31
#define ARGON_OBJECT_HASH_PRIME 2147483647 // pow(2, 31) - 1
#else
#define ARGON_OBJECT_HASH_BITS  61
#define ARGON_OBJECT_HASH_PRIME 2305843009213693951 // pow(2, 61) - 1
#endif

#define ARGON_OBJECT_HASH_NAN   0x4E414E
#define ARGON_OBJECT_HASH_INF   0x4CB2F

namespace argon::vm::datatype {
    ArSize HashBytes(const unsigned char *bytes, ArSize size);
}

#endif // !ARGON_VM_DATATYPE_HASH_MAGIC_H_
