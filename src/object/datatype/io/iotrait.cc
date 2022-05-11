// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "io.h"

using namespace argon::object;
using namespace argon::object::io;

const NativeFunc read_t_methods[] = {
        ARGON_METHOD_STUB("read",
                          "Read up to size bytes and return them."
                          ""
                          "As a convenience, if size is -1, all bytes until EOF are returned"
                          "With size = -1, read() may be using multiple calls to the stream."
                          ""
                          "- Parameter size: number of bytes to read from the stream."
                          "- Returns: (bytes, err)", 1, false),
        ARGON_METHOD_STUB("readinto",
                          "Read bytes into a pre-allocated, writable bytes-like object."
                          ""
                          "- Parameter obj: bytes-like writable object."
                          "- Returns: (bytes read, err)", 1, false),
        ARGON_METHOD_SENTINEL
};

const NativeFunc write_t_methods[] = {
        ARGON_METHOD_STUB("write",
                          "Write a bytes-like object to underlying stream."
                          ""
                          "- Parameter obj: bytes-like object to write to."
                          "- Returns: (bytes written, err)", 1, false),
        ARGON_METHOD_SENTINEL
};

const ObjectSlots read_t_obj = {
        read_t_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

const ObjectSlots write_t_obj = {
        write_t_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

const TypeInfo ReadTrait = {
        TYPEINFO_STATIC_INIT,
        "Read",
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
        nullptr,
        nullptr,
        &read_t_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::io::type_readT_ = &ReadTrait;

const TypeInfo WriteTrait = {
        TYPEINFO_STATIC_INIT,
        "Write",
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
        nullptr,
        nullptr,
        &write_t_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::io::type_writeT_ = &WriteTrait;
