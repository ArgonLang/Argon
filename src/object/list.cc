// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "list.h"

#include <memory/memory.h>

using namespace argon::object;
using namespace argon::memory;

List::List() : List(ARGON_OBJECT_LIST_INITIAL_CAP) {}

List::List(size_t capacity) : cap_(capacity) {
    this->objects_ = (Object **) Alloc(capacity * sizeof(Object *));
    this->len_ = 0;
}

List::~List() {
    this->Clear();
    argon::memory::Free(this->objects_);
}

void List::Append(Object *item) {
    this->CheckSize();
    IncStrongRef(item);
    this->objects_[this->len_] = item;
    this->len_++;
}

void List::CheckSize() {
    Object **tmp = nullptr;

    if (this->len_ + 1 >= this->cap_) {
        tmp = (Object **) Realloc(this->objects_, (this->cap_ + (this->cap_ / 2)) * sizeof(Object *));
        assert(tmp != nullptr); // TODO: NOMEM
        this->objects_ = tmp;
        this->cap_ += this->cap_ / 2;
    }
}

void List::Clear() {
    for (int i = 0; i < this->len_; i++)
        ReleaseObject(this->objects_[i]);
    this->len_ = 0;
}

Object *List::At(size_t index) {
    //if(index>=this->len_)
    //    throw //TODO: ArrayOutOfBound

    return this->objects_[index];
}

size_t List::Count() {
    return this->len_;
}
