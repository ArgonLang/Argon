// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BYTES_H_
#define ARGON_OBJECT_BYTES_H_

#include <object/arobject.h>
#include <object/bufview.h>
#include <object/rwlock.h>

#define ARGON_OBJECT_BYTES_INITIAL_CAP   16

namespace argon::object {
    struct Bytes : ArObject {
        RWLock lock;
        BufferView view;
        ArSize hash;

        bool frozen;
    };

    extern const TypeInfo *type_bytes_;

    Bytes *BytesNew(ArObject *object);

    Bytes *BytesNew(ArSize cap, bool same_len, bool fill_zero, bool frozen);

    Bytes *BytesNew(unsigned char *buffer, ArSize len, bool frozen);

    Bytes *BytesNewHoldBuffer(unsigned char *buffer, ArSize len, ArSize cap, bool frozen);

    Bytes *BytesFreeze(Bytes *stream);

    inline Bytes *BytesNew() {
        return BytesNew(ARGON_OBJECT_BYTES_INITIAL_CAP, false, false, false);
    }

    ArObject *BytesSplit(Bytes *bytes, unsigned char *pattern, ArSize plen, ArSSize maxsplit);

} // namespace argon::object


#endif // !ARGON_OBJECT_BYTES_H_
