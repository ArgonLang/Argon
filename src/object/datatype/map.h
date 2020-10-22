// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_MAP_H_
#define ARGON_OBJECT_MAP_H_

#include <object/arobject.h>

#define ARGON_OBJECT_MAP_INITIAL_SIZE   10
#define ARGON_OBJECT_MAP_LOAD_FACTOR    0.75f
#define ARGON_OBJECT_MAP_MUL_FACTOR     (ARGON_OBJECT_MAP_LOAD_FACTOR * 2)

namespace argon::object {
    struct MapEntry {
        MapEntry *next = nullptr;

        MapEntry *iter_next = nullptr;
        MapEntry *iter_prev = nullptr;

        ArObject *key = nullptr;
        ArObject *value = nullptr;
    };

    struct Map : ArObject {
        MapEntry **map;
        MapEntry *free_node;
        MapEntry *iter_begin;
        MapEntry *iter_end;

        size_t cap;
        size_t len;
    };

    Map *MapNew();

    ArObject *MapGet(Map *map, ArObject *key);

    ArObject *MapGetFrmStr(Map *map, const char *key, size_t len);

    bool MapInsert(Map *map, ArObject *key, ArObject *value);

    void MapRemove(Map *map, ArObject *key);

    bool MapContains(Map *map, ArObject *key);

} // namespace argon::object

#endif // !ARGON_OBJECT_MAP_H_
