// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BYTES_H_
#define ARGON_OBJECT_BYTES_H_

#include <string>

#include <object/arobject.h>
#include <object/bufview.h>

namespace argon::object {

    struct Bytes : ArObject {
        BufferView view;
        ArSize hash;
    };

    extern const TypeInfo type_bytes_;

    Bytes *BytesNew(ArSize len);

    Bytes *BytesNew(ArObject *object);

    Bytes *BytesNew(Bytes *bytes, ArSize start, ArSize len);

    Bytes *BytesNew(const std::string &string);

    inline Bytes *BytesNew() { return BytesNew((size_t)0); }

} // namespace argon::object

#endif // !ARGON_OBJECT_BYTES_H_
