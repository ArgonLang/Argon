// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_MAP_H_
#define ARGON_OBJECT_MAP_H_

#include <object/arobject.h>

#include "hmap.h"

namespace argon::object {
    struct MapEntry : HEntry {
        ArObject *value;
    };

    struct Map : ArObject {
        HMap hmap;
    };

    extern const TypeInfo type_map_;

    Map *MapNew();

    ArObject *MapGet(Map *map, ArObject *key);

    ArObject *MapGetFrmStr(Map *map, const char *key, size_t len);

    bool MapInsert(Map *map, ArObject *key, ArObject *value);

    bool MapRemove(Map *map, ArObject *key);

} // namespace argon::object

#endif // !ARGON_OBJECT_MAP_H_
