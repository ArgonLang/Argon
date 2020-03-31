// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_MAP_H_
#define ARGON_OBJECT_MAP_H_

#include "object.h"

#define ARGON_OBJECT_MAP_INITIAL_SIZE   10
#define ARGON_OBJECT_MAP_LOAD_FACTOR    0.75f
#define ARGON_OBJECT_MAP_MUL_FACTOR     (ARGON_OBJECT_MAP_LOAD_FACTOR * 2)

namespace argon::object {
    struct MapEntry {
        MapEntry *next = nullptr;

        MapEntry *iter_next = nullptr;
        MapEntry *iter_prev = nullptr;

        Object *key = nullptr;
        Object *value = nullptr;
    };

    class Map : Object {
        MapEntry **map_;
        MapEntry *free_node_;
        MapEntry *iter_begin;
        MapEntry *iter_end;

        size_t cap_;
        size_t len_;

        void CheckSize();

        void AppendIterItem(MapEntry *entry);

        void RemoveIterItem(MapEntry *entry);

        MapEntry *FindOrAllocNode();

        void MoveToFreeNode(MapEntry *entry);

    public:
        Map();

        ~Map() override;

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

        void Insert(Object *key, Object *value);

        void Remove(Object *key);

        Object *GetItem(Object *key);
    };

    inline const TypeInfo type_map_ = {
            .name=(const unsigned char *) "map",
            .size=sizeof(Map)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_MAP_H_
