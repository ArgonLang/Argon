// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "string.h"

using namespace argon::object;

String::String(const std::string &str) {
    this->buffer_ = (unsigned char *) argon::memory::Alloc(str.length());
    for (size_t i = 0; i < str.length(); i++)
        this->buffer_[i] = str.c_str()[i];
    this->len_ = str.length();
}

String::~String() {
    argon::memory::Free(this->buffer_);
}
