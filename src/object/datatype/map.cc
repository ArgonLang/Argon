// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include <object/arobject.h>

#include "hash_magic.h"
#include "error.h"
#include "map.h"

using namespace argon::object;
using namespace argon::memory;

size_t map_len(Map *self) {
    return self->hmap.len;
}

ArObject *argon::object::MapGet(Map *map, ArObject *key) {
    MapEntry *entry;

    if (!IsHashable(key))
        return nullptr;

    if ((entry = (MapEntry *) HMapLookup(&map->hmap, key))) {
        IncRef(entry->value);
        return entry->value;
    }

    return nullptr;
}

bool argon::object::MapInsert(Map *map, ArObject *key, ArObject *value) {
    MapEntry *entry;

    if (!IsHashable(key))
        return false;

    if ((entry = (MapEntry *) HMapLookup(&map->hmap, key)) != nullptr) {
        Release(entry->value);
        IncRef(value);
        entry->value = value;
        return true;
    }

    if ((entry = HMapFindOrAllocNode<MapEntry>(&map->hmap)) == nullptr) {
        argon::vm::Panic(OutOfMemoryError);
        return false;
    }

    IncRef(key);
    entry->key = key;
    IncRef(value);
    entry->value = value;

    if (!HMapInsert(&map->hmap, entry)) {
        Release(key);
        Release(value);
        HMapEntryToFreeNode(&map->hmap, entry);
        argon::vm::Panic(OutOfMemoryError);
        return false;
    }

    return true;
}

const MapSlots map_actions{
        (SizeTUnaryOp) map_len,
        (BinaryOp) argon::object::MapGet,
        (BoolTernOp) argon::object::MapInsert
};

bool map_is_true(Map *self) {
    return self->hmap.len > 0;
}

bool map_equal(Map *self, ArObject *other) {
    return false;
}

ArObject *map_str(Map *self) {
    return nullptr;
}

void map_trace(Map *self, VoidUnaryOp trace) {
    for (auto *cur = (MapEntry *) self->hmap.iter_begin; cur != nullptr; cur = (MapEntry *) cur->iter_next)
        trace(cur->value);
}

void map_cleanup(Map *self) {
    HMapFinalize(&self->hmap, [](HEntry *entry) {
        Release(((MapEntry *) entry)->value);
    });
}

ArObject *argon::object::MapGetFrmStr(Map *map, const char *key, size_t len) {
    auto *entry = (MapEntry *) HMapLookup(&map->hmap, key, len);

    if (entry != nullptr) {
        IncRef(entry->value);
        return entry->value;
    }

    return nullptr;
}

bool argon::object::MapRemove(Map *map, ArObject *key) {
    MapEntry *entry;

    if (!IsHashable(key))
        return false;

    if ((entry = (MapEntry *) HMapRemove(&map->hmap, key)) != nullptr) {
        Release(entry->key);
        Release(entry->value);
        HMapEntryToFreeNode(&map->hmap, entry);
    }

    return entry != nullptr;
}

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
        (BoolBinOp) map_equal,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (Trace) map_trace,
        (VoidUnaryOp) map_cleanup
};

Map *argon::object::MapNew() {
    auto map = ArObjectGCNew<Map>(&type_map_);

    if (map != nullptr) {
        if (!HMapInit(&map->hmap))
            Release((ArObject **) &map);
    }

    return map;
}
