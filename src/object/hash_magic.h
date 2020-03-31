// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_HASH_MAGIC_H_
#define ARGON_OBJECT_HASH_MAGIC_H_

#include <cstddef>

namespace argon::object {
    size_t HashBytes(const unsigned char *bytes, size_t size);
}

#endif // !ARGON_OBJECT_HASH_MAGIC_H_
