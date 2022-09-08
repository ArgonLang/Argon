// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_BYTES_H_
#define ARGON_VM_DATATYPE_BYTES_H_

#include <cstring>

#include "arobject.h"
#include "bufview.h"

namespace argon::vm::datatype {
    struct Bytes {
        AROBJ_HEAD;

        BufferView view;

        ArSize hash;

        bool frozen;
    };
    extern const TypeInfo *type_bytes_;

    Bytes *BytesNew(ArSize cap, bool same_len, bool fill_zero, bool frozen);

    Bytes *BytesNew(const unsigned char *buffer, ArSize len, bool frozen);

    inline Bytes *BytesNew(const char *string, bool frozen) {
        return BytesNew((const unsigned char *) string, strlen(string), frozen);
    }
}

#endif // !ARGON_VM_DATATYPE_BYTES_H_
