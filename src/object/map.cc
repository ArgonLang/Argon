// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <memory/memory.h>
#include "map.h"
#include "nil.h"

using namespace argon::object;
using namespace argon::memory;

Map::Map() : Object(&type_map_) {
    this->map_ = (MapEntry **) Alloc(ARGON_OBJECT_MAP_INITIAL_SIZE * sizeof(MapEntry *));
    assert(this->map_ != nullptr); // TODO: NOMEM

    this->free_node_ = nullptr;
    this->iter_begin = nullptr;
    this->iter_end = nullptr;

    this->cap_ = ARGON_OBJECT_MAP_INITIAL_SIZE;
    this->len_ = 0;

    for (size_t i = 0; i < this->cap_; i++)
        this->map_[i] = nullptr;
}

void Map::Insert(Object *key, Object *value) {
    MapEntry *entry;
    size_t index;

    this->CheckSize();

    index = key->Hash() % this->cap_;
    for (entry = this->map_[index]; entry != nullptr; entry = entry->next) {
        if (key->EqualTo(entry->key)) {
            ReleaseObject(entry->value); // release old value
            break;
        }
    }

    if (entry == nullptr) {
        entry = this->FindOrAllocNode();
        IncStrongRef(key);
        entry->key = key;

        entry->next = this->map_[index];
        this->map_[index] = entry;

        this->AppendIterItem(entry);

        this->len_++;
    }

    IncStrongRef(value);
    entry->value = value;
}

Map::~Map() {
    MapEntry *tmp = nullptr;

    for (MapEntry *cur = this->iter_begin; cur != nullptr; cur = tmp) {
        ReleaseObject(cur->key);
        ReleaseObject(cur->value);
        tmp = cur->iter_next;
        FreeObject<MapEntry>(cur);
    }

    for (MapEntry *cur = this->free_node_; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        FreeObject<MapEntry>(cur);
    }

    Free(this->map_);
}

void Map::Remove(Object *key) {
    size_t index = key->Hash() % this->cap_;

    for (MapEntry *cur = this->map_[index]; cur != nullptr; cur = cur->next) {
        if (key->EqualTo(cur->key)) {
            ReleaseObject(cur->key);
            ReleaseObject(cur->value);
            this->RemoveIterItem(cur);
            this->MoveToFreeNode(cur);
            this->len_--;
            break;
        }
    }
}

MapEntry *Map::FindOrAllocNode() {
    MapEntry *entry;

    if (this->free_node_ == nullptr) {
        entry = AllocObject<MapEntry>();
        assert(entry != nullptr); // TODO NOMEM
        return entry;
    }

    entry = this->free_node_;
    this->free_node_ = entry->next;
    return entry;
}

void Map::MoveToFreeNode(MapEntry *entry) {
    entry->next = this->free_node_;
    this->free_node_ = entry;
}

void Map::CheckSize() {
    MapEntry **new_map = nullptr;
    size_t new_cap;

    if (((float) this->len_ + 1) / this->cap_ < ARGON_OBJECT_MAP_LOAD_FACTOR)
        return;

    new_cap = this->cap_ + (this->cap_ / ARGON_OBJECT_MAP_MUL_FACTOR);

    new_map = (MapEntry **) Realloc(this->map_, new_cap * sizeof(MapEntry *));
    assert(new_map != nullptr); //TODO: NOMEM

    for (size_t i = this->cap_; i < new_cap; i++)
        new_map[i] = nullptr;

    for (size_t i = 0; i < new_cap; i++) {
        for (MapEntry *prev = nullptr, *cur = new_map[i], *next; cur != nullptr; cur = next) {
            size_t hash = cur->key->Hash() % new_cap;
            next = cur->next;

            if (hash == i) {
                prev = cur;
                continue;
            }

            cur->next = new_map[i];
            new_map[i] = cur->next;
            if (prev != nullptr)
                prev->next = next;
            else
                new_map[i] = next;
        }
    }

    this->map_ = new_map;
    this->cap_ = new_cap;
}

bool Map::EqualTo(const Object *other) {
    return false;
}

size_t Map::Hash() {
    return 0;
}

void Map::AppendIterItem(MapEntry *entry) {
    if (this->iter_begin == nullptr) {
        this->iter_begin = entry;
        this->iter_end = entry;
        return;
    }

    entry->iter_next = nullptr;
    entry->iter_prev = this->iter_end;
    this->iter_end->iter_next = entry;
    this->iter_end = entry;
}

void Map::RemoveIterItem(MapEntry *entry) {
    if (entry->iter_prev != nullptr)
        entry->iter_prev->iter_next = entry->iter_next;
    else
        this->iter_begin = entry->iter_next;

    if (entry->iter_next != nullptr)
        entry->iter_next->iter_prev = entry->iter_prev;
    else
        this->iter_end = entry->iter_prev;
}

Object *Map::GetItem(Object *key) {
    size_t index = key->Hash() % this->cap_;

    for (MapEntry *cur = this->map_[index]; cur != nullptr; cur = cur->next) {
        if (key->EqualTo(cur->key))
            return cur->value;
    }

    return Nil::NilValue();
}

void Map::Clear() {
    for (MapEntry *cur = this->iter_begin; cur != nullptr; cur = cur->iter_next) {
        ReleaseObject(cur->key);
        ReleaseObject(cur->value);
        this->RemoveIterItem(cur);
        this->MoveToFreeNode(cur);
    }
    this->len_ = 0;
}

bool Map::Contains(Object *key) {
    size_t index = key->Hash() % this->cap_;

    for (MapEntry *cur = this->map_[index]; cur != nullptr; cur = cur->next)
        if (key->EqualTo(cur->key))
            return true;

    return false;
}