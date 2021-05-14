// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "hash_magic.h"

// DJB33A
size_t argon::object::HashBytes(const unsigned char *bytes, size_t size) {
    size_t hash_value = 5381; // DJBX33A starts with 5381

    for (size_t i = 0; i < size; i++)
        hash_value = ((hash_value << (unsigned char) 5) + hash_value) + bytes[i];

    return hash_value;
}

