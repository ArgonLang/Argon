// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BYTES_H_
#define ARGON_OBJECT_BYTES_H_

#include <string>
#include <object/arobject.h>

namespace argon::object {

    struct Bytes : ArObject {
        unsigned char *buffer;
        size_t len;
        size_t hash;
    };

    extern const TypeInfo type_bytes_;

    Bytes *BytesNew(size_t len);

    Bytes *BytesNew(ArObject *object);

    Bytes *BytesNew(const std::string &string);

    inline Bytes *BytesNew() { return BytesNew((size_t)0); }

} // namespace argon::object

#endif // !ARGON_OBJECT_BYTES_H_
