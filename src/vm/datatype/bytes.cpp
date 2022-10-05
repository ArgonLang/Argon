// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bytes.h"

#define BUFFER_GET(bs)              ((bs)->view.buffer)
#define BUFFER_LEN(bs)              ((bs)->view.length)

using namespace argon::vm::datatype;

TypeInfo BytesType = {
        AROBJ_HEAD_INIT_TYPE,
        "Bytes",
        nullptr,
        nullptr,
        sizeof(Bytes),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_bytes_ = &BytesType;

Bytes *argon::vm::datatype::BytesNew(ArSize cap, bool same_len, bool fill_zero, bool frozen) {
    auto *bs = MakeObject<Bytes>(&BytesType);

    if (bs != nullptr) {
        if (!BufferViewInit(&bs->view, cap)) {
            Release(bs);
            return nullptr;
        }

        if (same_len)
            bs->view.length = cap;

        if (fill_zero)
            memory::MemoryZero(BUFFER_GET(bs), cap);

        bs->hash = 0;
        bs->frozen = frozen;
    }

    return bs;
}

Bytes *argon::vm::datatype::BytesNew(const unsigned char *buffer, ArSize len, bool frozen) {
    auto *bs = BytesNew(len, true, false, frozen);

    if (bs != nullptr)
        memory::MemoryCopy(BUFFER_GET(bs), buffer, len);

    return bs;
}