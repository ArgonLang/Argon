// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include <memory/memory.h>
#include <object/arobject.h>
#include "hash_magic.h"
#include "string.h"
#include "map.h"

using namespace argon::object;
using namespace argon::memory;

bool map_equal(ArObject *self, ArObject *other) {
    return false;
}

size_t map_hash(ArObject *obj) {
    return 0;
}

size_t map_len(ArObject *obj) {
    return ((Map *) obj)->len;
}

void map_cleanup(ArObject *obj) {
    auto map = (Map *) obj;
    MapEntry *tmp = nullptr;

    for (MapEntry *cur = map->iter_begin; cur != nullptr; cur = tmp) {
        Release(cur->key);
        Release(cur->value);
        tmp = cur->iter_next;
        FreeObject<MapEntry>(cur);
    }

    for (MapEntry *cur = map->free_node; cur != nullptr; cur = tmp) {
        tmp = cur->next;
        FreeObject<MapEntry>(cur);
    }

    Free(map->map);
}

void map_trace(Map *self, VoidUnaryOp trace) {
    for (MapEntry *cur = self->iter_begin; cur != nullptr; cur = cur->iter_next)
        trace(cur->value);
}

bool CheckSize(Map *map) {
    MapEntry **new_map = nullptr;
    size_t new_cap;

    if (((float) map->len + 1) / map->cap < ARGON_OBJECT_MAP_LOAD_FACTOR)
        return true;

    new_cap = map->cap + (map->cap / ARGON_OBJECT_MAP_MUL_FACTOR);

    new_map = (MapEntry **) Realloc(map->map, new_cap * sizeof(MapEntry *));
    if (new_map == nullptr)
        return false;

    for (size_t i = map->cap; i < new_cap; i++)
        new_map[i] = nullptr;

    for (size_t i = 0; i < map->cap; i++) {
        for (MapEntry *prev = nullptr, *cur = new_map[i], *next; cur != nullptr; cur = next) {
            size_t hash = cur->key->type->hash(cur->key) % new_cap;
            next = cur->next;

            if (hash == i) {
                prev = cur;
                continue;
            }

            cur->next = new_map[hash];
            new_map[hash] = cur;
            if (prev != nullptr)
                prev->next = next;
            else
                new_map[i] = next;
        }
    }

    map->map = new_map;
    map->cap = new_cap;

    return true;
}

MapEntry *FindOrAllocNode(Map *map) {
    MapEntry *entry;

    if (map->free_node == nullptr) {
        entry = AllocObject<MapEntry>();
        assert(entry != nullptr); // TODO NOMEM
        return entry;
    }

    entry = map->free_node;
    map->free_node = entry->next;
    return entry;
}

inline void MoveToFreeNode(Map *map, MapEntry *entry) {
    entry->next = map->free_node;
    map->free_node = entry;
}

void AppendIterItem(Map *map, MapEntry *entry) {
    if (map->iter_begin == nullptr) {
        map->iter_begin = entry;
        map->iter_end = entry;
        return;
    }

    entry->iter_next = nullptr;
    entry->iter_prev = map->iter_end;
    map->iter_end->iter_next = entry;
    map->iter_end = entry;
}

void RemoveIterItem(Map *map, MapEntry *entry) {
    if (entry->iter_prev != nullptr)
        entry->iter_prev->iter_next = entry->iter_next;
    else
        map->iter_begin = entry->iter_next;

    if (entry->iter_next != nullptr)
        entry->iter_next->iter_prev = entry->iter_prev;
    else
        map->iter_end = entry->iter_prev;
}

ArObject *argon::object::MapGet(Map *map, ArObject *key) {
    size_t index = key->type->hash(key) % map->cap;

    for (MapEntry *cur = map->map[index]; cur != nullptr; cur = cur->next) {
        if (key->type->equal(key, cur->key)) {
            IncRef(cur->value);
            return cur->value;
        }
    }

    return nullptr;
}

ArObject *argon::object::MapGetFrmStr(Map *map, const char *key, size_t len) {
    size_t index = HashBytes((const unsigned char *) key, len) % map->cap;

    for (MapEntry *cur = map->map[index]; cur != nullptr; cur = cur->next) {
        if (cur->key->type == &type_string_ && StringEq((String *) cur->key, (const unsigned char *) key, len)) {
            IncRef(cur->value);
            return cur->value;
        }
    }

    return nullptr;
}

bool argon::object::MapInsert(Map *map, ArObject *key, ArObject *value) {
    MapEntry *entry;
    size_t index;

    if (!CheckSize(map)) {
        assert(false); //TODO: NOMEM
        return false;
    }

    index = key->type->hash(key) % map->cap;
    for (entry = map->map[index]; entry != nullptr; entry = entry->next) {
        if (key->type->equal(key, entry->key)) {
            Release(entry->value); // release old value
            break;
        }
    }

    if (entry == nullptr) {
        entry = FindOrAllocNode(map);
        IncRef(key);
        entry->key = key;

        entry->next = map->map[index];
        map->map[index] = entry;

        AppendIterItem(map, entry);

        map->len++;
    }

    IncRef(value);
    entry->value = value;

    return true;
}

void argon::object::MapRemove(Map *map, ArObject *key) {
    size_t index = key->type->hash(key) % map->cap;

    for (MapEntry *cur = map->map[index]; cur != nullptr; cur = cur->next) {
        if (key->type->equal(key, cur->key)) {
            Release(cur->key);
            Release(cur->value);
            RemoveIterItem(map, cur);
            MoveToFreeNode(map, cur);
            map->len--;
            break;
        }
    }
}

bool argon::object::MapContains(Map *map, ArObject *key) {
    size_t index = key->type->hash(key) % map->cap;

    for (MapEntry *cur = map->map[index]; cur != nullptr; cur = cur->next)
        if (key->type->equal(key, cur->key))
            return true;

    return false;
}

const MapSlots map_actions{
        map_len,
        (BinaryOp) argon::object::MapGet,
        (BoolTernOp) argon::object::MapInsert
};

const TypeInfo argon::object::type_map_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "map",
        sizeof(Map),
        nullptr,
        nullptr,
        &map_actions,
        nullptr,
        nullptr,
        nullptr,
        map_equal,
        nullptr,
        map_hash,
        nullptr,
        nullptr,
        (Trace) map_trace,
        map_cleanup
};

Map *argon::object::MapNew() {
    auto map = ArObjectGCNew<Map>(&type_map_);
    assert(map != nullptr);


    map->map = (MapEntry **) Alloc(ARGON_OBJECT_MAP_INITIAL_SIZE * sizeof(MapEntry *));
    assert(map->map != nullptr); // TODO: NOMEM

    map->free_node = nullptr;
    map->iter_begin = nullptr;
    map->iter_end = nullptr;

    map->cap = ARGON_OBJECT_MAP_INITIAL_SIZE;
    map->len = 0;

    for (size_t i = 0; i < map->cap; i++)
        map->map[i] = nullptr;

    return map;
}

/*
void Map::Clear() {
    for (MapEntry *cur = this->iter_begin; cur != nullptr; cur = cur->iter_next) {
        ReleaseObject(cur->key);
        ReleaseObject(cur->value);
        this->RemoveIterItem(cur);
        this->MoveToFreeNode(cur);
    }
    this->len_ = 0;
}*/