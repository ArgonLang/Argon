// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "string.h"
#include "hash_magic.h"

using namespace argon::object;

String::String(const std::string &str) : Object(&type_string_) {
    this->buffer_ = (unsigned char *) argon::memory::Alloc(str.length());
    for (size_t i = 0; i < str.length(); i++)
        this->buffer_[i] = str.c_str()[i];
    this->len_ = str.length();
    this->hash_ = 0;
}

String::~String() {
    argon::memory::Free(this->buffer_);
}

bool String::EqualTo(const Object *other) {
    if (this != other) {
        if (this->type != other->type)
            return false;

        auto tmp = (String *) other;

        if (this->len_ != tmp->len_)
            return false;

        for (size_t i = 0; i < this->len_; i++) {
            if (this->buffer_[i] != tmp->buffer_[i])
                return false;
        }
    }
    return true;
}

size_t String::Hash() {
    if (this->hash_ == 0)
        this->hash_ = HashBytes(this->buffer_, this->len_);
    return this->hash_;
}
