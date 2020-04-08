// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "list.h"

#include <memory/memory.h>

using namespace argon::object;
using namespace argon::memory;

List::List() : List(ARGON_OBJECT_LIST_INITIAL_CAP) {}

List::List(size_t capacity) : Sequence(&type_list_), cap_(capacity) {
    this->objects = (Object **) Alloc(capacity * sizeof(Object *));
    this->len = 0;
}

List::~List() {
    this->Clear();
    argon::memory::Free(this->objects);
}

void List::Append(Object *item) {
    this->CheckSize();
    IncStrongRef(item);
    this->objects[this->len] = item;
    this->len++;
}

void List::CheckSize() {
    Object **tmp = nullptr;

    if (this->len + 1 >= this->cap_) {
        tmp = (Object **) Realloc(this->objects, (this->cap_ + (this->cap_ / 2)) * sizeof(Object *));
        assert(tmp != nullptr); // TODO: NOMEM
        this->objects = tmp;
        this->cap_ += this->cap_ / 2;
    }
}

void List::Clear() {
    for (int i = 0; i < this->len; i++)
        ReleaseObject(this->objects[i]);
    this->len = 0;
}

bool List::EqualTo(const Object *other) {
    return false;
}

size_t List::Hash() {
    return 0;
}

Object *List::GetItem(const Object *i) {
    return nullptr;
}
