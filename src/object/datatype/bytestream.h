// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BYTESTREAM_H_
#define ARGON_OBJECT_BYTESTREAM_H_

#include <object/arobject.h>
#include <object/bufview.h>

#define ARGON_OBJECT_BYTESTREAM_INITIAL_CAP   16

namespace argon::object {
    struct ByteStream : ArObject {
        BufferView view;
    };

    extern const TypeInfo type_bytestream_;

    ByteStream *ByteStreamNew(ArObject *object);

    ByteStream *ByteStreamNew(ByteStream *stream, ArSize start, ArSize len);

    ByteStream *ByteStreamNew(ArSize cap, bool same_len, bool fill_zero);

    inline ByteStream *ByteStreamNew() { return ByteStreamNew(ARGON_OBJECT_BYTESTREAM_INITIAL_CAP, false, false); }

    ArObject *ByteStreamSplit(ByteStream *bytes, unsigned char *pattern, ArSize plen, ArSSize maxsplit);

} // namespace argon::object


#endif // !ARGON_OBJECT_BYTESTREAM_H_
