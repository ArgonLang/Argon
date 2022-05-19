// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/datatype/error.h>
#include <object/datatype/bytes.h>
#include <object/datatype/integer.h>
#include <object/datatype/function.h>

#include "io.h"

using namespace argon::object;
using namespace argon::object::io;

bool ReadFromBase(BufferedIO *bio) {
    ArObject *args[2] = {bio->base, bio->blocksz};
    ArObject *error;
    Bytes *bytes;
    Tuple *tp;

    if ((tp = (Tuple *) argon::vm::CallMethod(bio->base, "read", 2, args)) == nullptr)
        return false;

    if (!AR_TYPEOF(tp, type_tuple_)) {
        Release(tp);
        ErrorFormat(type_type_error_, "calling the %s::read method should return a tuple, not %s",
                    AR_TYPE_NAME(bio->base), AR_TYPE_NAME(tp));
        return false;
    }

    if (!TupleUnpack(tp, "aa", &bytes, &error)) {
        Release(tp);
        return false;
    }

    Release(tp);

    if (!IsNull(error)) {
        Release(bytes);
        argon::vm::Panic(error);
        Release(error);
        return false;
    }

    Release(error);

    if (!AR_TYPEOF(bytes, type_bytes_)) {
        Release(bytes);
        ErrorFormat(type_type_error_, "calling the %s::read method should return a tuple with (bytes, *), not (%s, *)",
                    AR_TYPE_NAME(bio->base), AR_TYPE_NAME(bytes));
        return false;
    }

    Release(bio->buffer);
    bio->buffer = bytes;
    bio->index = 0;

    return true;
}

bool WriteToBase(BufferedIO *bio, ArObject *bytes, ArObject **error) {
    ArObject *args[2] = {bio->base, bio->buffer};
    Integer *bw;
    Tuple *tp;

    bool ok = true;

    *error = nullptr;

    if (bytes != nullptr)
        args[1] = bytes;

    if ((tp = (Tuple *) argon::vm::CallMethod(bio->base, "write", 2, args)) == nullptr)
        return false;

    if (!AR_TYPEOF(tp, type_tuple_)) {
        Release(tp);
        ErrorFormat(type_type_error_, "calling the %s::write method should return a tuple, not %s",
                    AR_TYPE_NAME(bio->base), AR_TYPE_NAME(tp));
        return false;
    }

    if (!TupleUnpack(tp, "aa", &bw, error)) {
        Release(tp);
        return false;
    }

    if (!IsNull(*error))
        ok = false;
    else {
        Release(error);

        if (bytes == nullptr) {
            bio->index = 0;
            ((Bytes *) bio->buffer)->view.len = 0;
        }
    }

    Release(tp);
    Release(bw);

    return ok;
}

ArSSize ReadData(BufferedIO *bio, unsigned char *buffer, ArSize length) {
    std::unique_lock lock(bio->lock);
    const Bytes *biobuf = (Bytes *) bio->buffer;
    ArSize total = 0;
    ArSize rlen;

    while (length > 0) {
        if ((biobuf == nullptr || bio->index >= biobuf->view.len) && !ReadFromBase(bio))
            return -1;

        biobuf = (Bytes *) bio->buffer;

        rlen = length;
        if (rlen >= biobuf->view.len - bio->index)
            rlen = biobuf->view.len - bio->index;

        if (rlen == 0)
            break;

        argon::memory::MemoryCopy(buffer + total, biobuf->view.buffer + bio->index, rlen);

        bio->index += rlen;
        length -= rlen;
        total += rlen;
    }

    return (ArSSize) total;
}

ArSSize ReadLine(BufferedIO *bio, unsigned char *buffer, ArSize length) {
    std::unique_lock lock(bio->lock);
    const Bytes *biobuf = (Bytes *) bio->buffer;
    ArSize total = 0;
    ArSize rlen;
    ArSSize next;

    bool checknl = false;

    while (length > 0) {
        if ((biobuf == nullptr || bio->index >= biobuf->view.len) && !ReadFromBase(bio))
            return -1;

        biobuf = (Bytes *) bio->buffer;
        rlen = biobuf->view.len - bio->index;

        if (checknl) {
            if (*(biobuf->view.buffer + bio->index) == '\n')
                bio->index++;
            break;
        }

        if (biobuf->view.len == 0)
            return (ArSSize) total;

        if (rlen > length)
            rlen = length;

        if ((next = argon::object::support::FindNewLine(biobuf->view.buffer + bio->index, &rlen)) > 0) {
            argon::memory::MemoryCopy(buffer + total, biobuf->view.buffer + bio->index, rlen);
            bio->index += next;
            total += rlen;

            if (*(biobuf->view.buffer + bio->index - 1) == '\r') {
                // Check again in case of \r\n at the edge of buffer
                checknl = true;
                continue;
            }

            break;
        }

        argon::memory::MemoryCopy(buffer + total, biobuf->view.buffer + bio->index, rlen);
        bio->index += rlen;
        length -= rlen;
        total += rlen;
    }

    return (ArSSize) total;
}

ArObject *Read(BufferedIO *bio, ArObject *bytes, ArSSize cap) {
    ArBuffer buffer = {};
    unsigned char *raw;
    ArSSize len;

    if (cap <= 0)
        cap = ARGON_OBJECT_IO_DEFAULT_BUFFERED_CAP;

    if (bytes != nullptr) {
        if (!BufferGet(bytes, &buffer, ArBufferFlags::WRITE))
            return nullptr;

        raw = buffer.buffer;
        cap = (ArSSize) buffer.len;
    } else if ((raw = ArObjectNewRaw<unsigned char *>(cap)) == nullptr)
        return nullptr;

    if ((len = ReadData(bio, raw, cap)) < 0) {
        bytes != nullptr ? BufferRelease(&buffer) : argon::memory::Free(raw);
        return nullptr;
    }

    if (bytes != nullptr) {
        BufferRelease(&buffer);
        return IncRef(bytes);
    }

    if ((bytes = BytesNewHoldBuffer(raw, len, cap, true)) == nullptr)
        argon::memory::Free(raw);

    return bytes;
}

ARGON_FUNCTION5(buffered_, new, "", 2, false) {
    const TypeInfo *base = ((Function *) func)->base;
    const auto *arint = (Integer *) argv[1];
    BufferedIO *bio;

    ArSSize buflen = ARGON_OBJECT_IO_DEFAULT_BUFSIZE;

    if ((base == type_buffered_reader_ && !TraitIsImplemented(argv[0], type_readT_)) ||
        (base == type_buffered_writer_ && !TraitIsImplemented(argv[0], type_writeT_))) {
        return ErrorFormat(type_type_error_, "%s requires an object that implements %s", base->name,
                           type_readT_->name);
    }

    if (!CheckArgs("i:buflen", func, argv + 1, count - 1))
        return nullptr;

    if ((bio = ArObjectNew<BufferedIO>(RCType::INLINE, base)) == nullptr)
        return nullptr;

    bio->base = IncRef(argv[0]);

    new(&bio->lock)std::mutex();

    if (arint->integer > 0)
        buflen = arint->integer;

    bio->buffer = nullptr;
    if (base == type_buffered_writer_) {
        bio->buffer = BytesNew(buflen, false, false, false);
        if (bio->buffer == nullptr) {
            Release(bio);
            return nullptr;
        }
    }

    if ((bio->blocksz = IntegerNew(buflen)) == nullptr) {
        Release(bio);
        return nullptr;
    }

    bio->index = 0;

    return bio;
}

ARGON_METHOD5(buffered_, read, "", 1, false) {
    auto *bio = (BufferedIO *) self;
    ArObject *result;

    if (!CheckArgs("i:size", func, argv, count))
        return nullptr;

    if ((result = Read(bio, nullptr, ((Integer *) argv[0])->integer)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return ARGON_OBJECT_TUPLE_SUCCESS(result);
}

ARGON_METHOD5(buffered_, readinto, "", 1, false) {
    auto *bio = (BufferedIO *) self;
    ArObject *result;

    if (!CheckArgs("B:buffer", func, argv, count))
        return nullptr;

    if ((result = Read(bio, argv[0], 0)) == nullptr)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return ARGON_OBJECT_TUPLE_SUCCESS(result);
}

ARGON_METHOD5(buffered_, readline, "", 1, false) {
    auto *bio = (BufferedIO *) self;
    const auto *arint = (Integer *) argv[0];

    unsigned char *buffer;
    Bytes *bytes;

    ArSize cap = ARGON_OBJECT_IO_DEFAULT_BUFFERED_CAP;
    ArSSize len;

    if (!CheckArgs("i:size", func, argv, count))
        return nullptr;

    if (arint->integer > 0)
        cap = arint->integer;

    if ((buffer = ArObjectNewRaw<unsigned char *>(cap)) == nullptr)
        return nullptr;

    if ((len = ReadLine(bio, buffer, cap)) < 0) {
        argon::memory::Free(buffer);
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    if ((bytes = BytesNewHoldBuffer(buffer, len, cap, true)) == nullptr) {
        argon::memory::Free(buffer);
        return nullptr;
    }

    return ARGON_OBJECT_TUPLE_SUCCESS(bytes);
}

ARGON_METHOD5(buffered_, write, "", 1, false) {
    ArBuffer buffer = {};
    auto *bio = (BufferedIO *) self;
    ArObject *error = nullptr;
    ArObject *res;
    Bytes *biobuf;
    ArSize blocksz;
    ArSize widx;

    std::unique_lock lock(bio->lock);

    biobuf = (Bytes *) bio->buffer;
    blocksz = biobuf->view.shared->cap;
    widx = 0;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    if (bio->index == 0 && buffer.len >= blocksz) {
        WriteToBase(bio, argv[0], &error);
        widx = buffer.len;
    }

    while (widx < buffer.len) {
        ArSize tow = buffer.len - widx;

        if (tow > blocksz - bio->index)
            tow = blocksz - bio->index;

        argon::memory::MemoryCopy(biobuf->view.buffer + bio->index, buffer.buffer + widx, tow);

        biobuf->view.len += tow;
        bio->index += tow;
        widx += tow;

        // Flush ?
        if (bio->index == blocksz && !WriteToBase(bio, nullptr, &error))
            break;
    }

    BufferRelease(&buffer);

    if ((res = TupleNew("ia", widx, error)) == nullptr)
        return nullptr;

    return res;
}

ARGON_METHOD5(buffered_, flush, "", 0, false) {
    ArObject *error;

    if (!WriteToBase((BufferedIO *) self, nullptr, &error))
        return error;

    return ARGON_OBJECT_NIL;
}

const NativeFunc buffered_reader_methods[] = {
        buffered_new_,
        buffered_read_,
        buffered_readinto_,
        buffered_readline_,
        ARGON_METHOD_SENTINEL
};

const NativeFunc buffered_writer_methods[] = {
        buffered_new_,
        buffered_write_,
        buffered_flush_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots buffered_reader_obj = {
        buffered_reader_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

const ObjectSlots buffered_writer_obj = {
        buffered_writer_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void buffer_cleanup(BufferedIO *self) {
    ArObject *error;

    if (AR_TYPEOF(self, type_buffered_writer_)) {
        // flush buffer and silently ignore errors
        WriteToBase(self, nullptr, &error);
        Release(error);
    }

    self->lock.~mutex();

    Release(self->base);
    Release(self->buffer);
    Release(self->blocksz);
}

const TypeInfo BufferedReader = {
        TYPEINFO_STATIC_INIT,
        "BufferedReader",
        nullptr,
        sizeof(BufferedIO),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) buffer_cleanup,
        nullptr,
        nullptr,
        TypeInfo_IsTrue_True,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &buffered_reader_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::io::type_buffered_reader_ = &BufferedReader;

const TypeInfo BufferedWriter = {
        TYPEINFO_STATIC_INIT,
        "BufferedWriter",
        nullptr,
        sizeof(BufferedIO),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) buffer_cleanup,
        nullptr,
        nullptr,
        TypeInfo_IsTrue_True,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &buffered_writer_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::io::type_buffered_writer_ = &BufferedWriter;
