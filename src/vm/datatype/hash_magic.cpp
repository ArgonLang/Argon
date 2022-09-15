// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "hash_magic.h"

using namespace argon::vm::datatype;

// DJB33A
ArSize argon::vm::datatype::HashBytes(const unsigned char *bytes, ArSize size) {
    ArSize hash_value = 5381; // DJBX33A starts with 5381

    for (ArSize i = 0; i < size; i++)
        hash_value = ((hash_value << (unsigned char) 5) + hash_value) + bytes[i];

    return hash_value;
}

