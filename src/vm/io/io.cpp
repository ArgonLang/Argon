// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "fio.h"
#include "io.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io;

const FunctionDef reader_methods[] = {
        ARGON_METHOD_STUB("read",
                          "Read up to size bytes from the stream and return them.\n"
                          "\n"
                          "As a convenience, if size is -1, all bytes until EOF are returned.\n"
                          "With size = -1, read() may be using multiple calls to the stream.\n"
                          "\n"
                          "- Parameter size: Number of bytes to read from the stream.\n"
                          "- Returns: Bytes object.\n",
                          "i: size", false, false),
        ARGON_METHOD_STUB("readinto",
                          "Read bytes into a pre-allocated, writable bytes-like object.\n"
                          "\n"
                          "- Parameters:\n"
                          "  - obj: Bytes-like writable object.\n"
                          "  - offset: Offset to start writing from.\n"
                          "- Returns: Number of bytes read.\n",
                          ": obj, i: offset", false, false),
        ARGON_METHOD_SENTINEL
};

const ObjectSlots reader_objslot = {
        reader_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

TypeInfo ReaderType = {
        AROBJ_HEAD_INIT_TYPE,
        "Reader",
        nullptr,
        nullptr,
        0,
        TypeInfoFlags::TRAIT,
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
        &reader_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::io::type_reader_t_ = &ReaderType;

const FunctionDef writer_methods[] = {
        ARGON_METHOD_STUB("write",
                          "Write a bytes-like object to underlying stream.\n"
                          "\n"
                          "- Parameter: obj: Bytes-like object to write to.\n"
                          "- Returns: Bytes written.\n",
                          ": obj", false, false),
        ARGON_METHOD_SENTINEL
};

const ObjectSlots writer_objslot = {
        writer_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

TypeInfo WriterType = {
        AROBJ_HEAD_INIT_TYPE,
        "Writer",
        nullptr,
        nullptr,
        0,
        TypeInfoFlags::TRAIT,
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
        &writer_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::io::type_writer_t_ = &WriterType;

bool argon::vm::io::IOInit() {
    if (!TypeInit((TypeInfo *) type_reader_t_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_writer_t_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_file_, nullptr))
        return false;

    return true;
}