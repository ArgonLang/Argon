// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "bool.h"
#include "error.h"
#include "hash_magic.h"
#include "list.h"
#include "option.h"
#include "tuple.h"
#include "map.h"

using namespace argon::object;

ArObject *map_iter_next(HMapIterator *iter) {
    Tuple *ret;

    if (iter->current == nullptr)
        return nullptr;

    if (iter->used != iter->map->len)
        return ErrorFormat(type_runtime_error_, "Map changed size during iteration");

    if ((ret = TupleNew(2)) != nullptr) {
        TupleInsertAt(ret, 0, iter->current->key);
        TupleInsertAt(ret, 1, ((MapEntry *) iter->current)->value);
        iter->current = iter->reversed ? iter->current->iter_prev : iter->current->iter_next;
    }

    return ret;
}

ArObject *map_iter_peak(HMapIterator *iter) {
    Tuple *ret;

    if (iter->current == nullptr)
        return nullptr;

    if (iter->used != iter->map->len)
        return ErrorFormat(type_runtime_error_, "Map changed size during iteration");

    if ((ret = TupleNew(2)) != nullptr) {
        TupleInsertAt(ret, 0, iter->current->key);
        TupleInsertAt(ret, 1, ((MapEntry *) iter->current)->value);
    }

    return ret;
}

const IteratorSlots map_iterop = {
        (BoolUnaryOp) HMapIteratorHasNext,
        (UnaryOp) map_iter_next,
        (UnaryOp) map_iter_peak,
        (VoidUnaryOp) HMapIteratorReset
};

const TypeInfo type_map_iterator_ = {
        TYPEINFO_STATIC_INIT,
        "map_iterator",
        nullptr,
        sizeof(HMapIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) HMapIteratorCleanup,
        (Trace) HMapIteratorTrace,
        (CompareOp)HMapIteratorCompare,
        (BoolUnaryOp) HMapIteratorHasNext,
        nullptr,
        (UnaryOp) HMapIteratorStr,
        nullptr,
        nullptr,
        nullptr,
        &map_iterop,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

size_t map_len(Map *self) {
    return self->hmap.len;
}

ArObject *argon::object::MapGet(Map *map, ArObject *key) {
    MapEntry *entry;

    if ((entry = (MapEntry *) HMapLookup(&map->hmap, key)) != nullptr) {
        IncRef(entry->value);
        return entry->value;
    }

    return nullptr;
}

bool argon::object::MapInsert(Map *map, ArObject *key, ArObject *value) {
    MapEntry *entry;

    if ((entry = (MapEntry *) HMapLookup(&map->hmap, key)) != nullptr) {
        Release(entry->value);
        IncRef(value);
        entry->value = value;
        return true;
    }

    // Check for UnashableError
    if (argon::vm::IsPanicking())
        return false;

    if ((entry = HMapFindOrAllocNode<MapEntry>(&map->hmap)) == nullptr)
        return false;

    IncRef(key);
    entry->key = key;
    IncRef(value);
    entry->value = value;

    if (!HMapInsert(&map->hmap, entry)) {
        Release(key);
        Release(value);
        HMapEntryToFreeNode(&map->hmap, entry);
        return false;
    }

    return true;
}

const MapSlots map_actions{
        (SizeTUnaryOp) map_len,
        (BinaryOp) argon::object::MapGet,
        (BoolTernOp) argon::object::MapInsert
};

ARGON_METHOD5(map_, clear,
              "Removes all the elements from the map."
              ""
              "- Returns: map itself.", 0, false) {
    MapClear((Map *) self);
    return IncRef(self);
}

ARGON_METHOD5(map_, get,
              "Returns the value of the specified key."
              ""
              "- Parameter key: map key."
              "- Returns: Option<?>.", 1, false) {
    MapEntry *entry;

    if ((entry = (MapEntry *) HMapLookup(&((Map *) self)->hmap, argv[0])) == nullptr) {
        if (argon::vm::IsPanicking())
            return nullptr;

        return OptionNew();
    }

    return OptionNew(entry->value);
}

ARGON_METHOD5(map_, items,
              "Returns a list containing a tuple for each key value pair."
              ""
              "- Returns: list containing a tuple for each key value pair.", 0, false) {
    auto *map = ((Map *) self);
    List *ret;
    Tuple *tmp;

    if ((ret = ListNew(map->hmap.len)) != nullptr) {
        for (auto *cur = (MapEntry *) map->hmap.iter_begin; cur != nullptr; cur = (MapEntry *) cur->iter_next) {
            if ((tmp = TupleNew(2)) == nullptr) {
                Release(ret);
                return nullptr;
            }

            TupleInsertAt(tmp, 0, cur->key);
            TupleInsertAt(tmp, 1, cur->value);

            ListAppend(ret, tmp);
            Release(tmp);
        }
    }

    return ret;
}

ARGON_METHOD5(map_, keys,
              "Returns a list containing the map's keys."
              ""
              "- Returns: list containing the map's keys", 0, false) {
    auto *map = ((Map *) self);
    List *ret;

    if ((ret = ListNew(map->hmap.len)) != nullptr) {
        for (auto *cur = (MapEntry *) map->hmap.iter_begin; cur != nullptr; cur = (MapEntry *) cur->iter_next)
            ListAppend(ret, cur->key);
    }

    return ret;
}

ARGON_METHOD5(map_, pop,
              "Removes the element with the specified key."
              ""
              "- Parameter key: map key."
              "- Returns: Option<?>.", 1, false) {
    auto *map = ((Map *) self);
    MapEntry *entry;
    Option *ret;

    if ((entry = (MapEntry *) HMapRemove(&map->hmap, argv[0])) != nullptr) {
        ret = OptionNew(entry->value);
        Release(entry->key);
        Release(entry->value);
        HMapEntryToFreeNode(&map->hmap, entry);
        return ret;
    }

    return OptionNew();
}

ARGON_METHOD5(map_, values, "Returns a list of all the values in the map."
                            ""
                            "- Returns: list of all the values in the map.", 0, false) {
    auto *map = ((Map *) self);
    List *ret;

    if ((ret = ListNew(map->hmap.len)) != nullptr) {
        for (auto *cur = (MapEntry *) map->hmap.iter_begin; cur != nullptr; cur = (MapEntry *) cur->iter_next)
            ListAppend(ret, cur->value);
    }

    return ret;
}

const NativeFunc map_methods[] = {
        map_clear_,
        map_get_,
        map_items_,
        map_keys_,
        map_pop_,
        map_values_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots map_obj = {
        map_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool map_is_true(Map *self) {
    return self->hmap.len > 0;
}

ArObject *map_compare(Map *self, ArObject *other, CompareMode mode) {
    auto *o = (Map *) other;
    MapEntry *cursor;
    MapEntry *tmp;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other) {
        for (cursor = (MapEntry *) self->hmap.iter_begin; cursor != nullptr; cursor = (MapEntry *) cursor->iter_next) {
            if ((tmp = (MapEntry *) HMapLookup(&o->hmap, cursor->key)) == nullptr)
                return BoolToArBool(false);

            if (!Equal(cursor->value, tmp->value))
                return BoolToArBool(false);
        }
    }

    return BoolToArBool(true);
}

ArObject *map_str(Map *self) {
    StringBuilder sb = {};
    String *tmp = nullptr;
    MapEntry *cursor;
    int rec;

    if ((rec = TrackRecursive(self)) != 0)
        return rec > 0 ? StringIntern("{...}") : nullptr;

    if (StringBuilderWrite(&sb, (unsigned char *) "{", 1, self->hmap.len == 0 ? 1 : 0) < 0)
        goto error;

    for (cursor = (MapEntry *) self->hmap.iter_begin; cursor != nullptr; cursor = (MapEntry *) cursor->iter_next) {
        if ((tmp = (String *) ToString(cursor->key)) == nullptr)
            goto error;

        if (StringBuilderWrite(&sb, tmp, 2) < 0)
            goto error;

        if (StringBuilderWrite(&sb, (unsigned char *) ": ", 2) < 0)
            goto error;

        Release(tmp);

        if ((tmp = (String *) ToString(cursor->value)) == nullptr)
            goto error;

        if (StringBuilderWrite(&sb, tmp, cursor->iter_next != nullptr ? 2 : 1) < 0)
            goto error;

        Release(tmp);

        if (cursor->iter_next != nullptr) {
            if (StringBuilderWrite(&sb, (unsigned char *) ", ", 2) < 0)
                goto error;
        }
    }

    if (StringBuilderWrite(&sb, (unsigned char *) "}", 1) < 0)
        goto error;

    UntrackRecursive(self);
    return StringBuilderFinish(&sb);

    error:
    Release(tmp);
    StringBuilderClean(&sb);
    UntrackRecursive(self);
    return nullptr;
}

ArObject *map_iter_get(Map *self) {
    return HMapIteratorNew(&type_map_iterator_, self, &self->hmap, false);
}

ArObject *map_iter_rget(Map *self) {
    return HMapIteratorNew(&type_map_iterator_, self, &self->hmap, true);
}

void map_trace(Map *self, VoidUnaryOp trace) {
    for (auto *cur = (MapEntry *) self->hmap.iter_begin; cur != nullptr; cur = (MapEntry *) cur->iter_next)
        trace(cur->value);
}

ArObject *map_ctor(ArObject **args, ArSize count) {
    if (!VariadicCheckPositional("map", count, 0, 1))
        return nullptr;

    if (count == 1)
        return MapNewFromIterable((const ArObject *) *args);

    return MapNew();
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

    if ((entry = (MapEntry *) HMapRemove(&map->hmap, key)) != nullptr) {
        Release(entry->key);
        Release(entry->value);
        HMapEntryToFreeNode(&map->hmap, entry);
    }

    return entry != nullptr;
}

const TypeInfo argon::object::type_map_ = {
        TYPEINFO_STATIC_INIT,
        "map",
        nullptr,
        sizeof(Map),
        TypeInfoFlags::BASE,
        map_ctor,
        (VoidUnaryOp) map_cleanup,
        (Trace) map_trace,
        (CompareOp) map_compare,
        (BoolUnaryOp) map_is_true,
        nullptr,
        (UnaryOp) map_str,
        (UnaryOp) map_iter_get,
        (UnaryOp) map_iter_rget,
        nullptr,
        nullptr,
        &map_actions,
        nullptr,
        &map_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

Map *argon::object::MapNew() {
    auto map = ArObjectGCNew<Map>(&type_map_);

    if (map != nullptr) {
        if (!HMapInit(&map->hmap))
            Release((ArObject **) &map);
    }

    return map;
}

Map *argon::object::MapNewFromIterable(const ArObject *iterable) {
    ArObject *iter;
    ArObject *key;
    ArObject *value;
    Map *map;

    bool ok;

    if (!IsIterable(iterable))
        return (Map *) ErrorFormat(type_type_error_, "'%s' is not iterable", AR_TYPE_NAME(iterable));

    if ((map = MapNew()) == nullptr)
        return nullptr;

    if ((iter = IteratorGet(iterable)) == nullptr) {
        Release(map);
        return nullptr;
    }

    while (true) {
        if ((key = IteratorNext(iter)) == nullptr)
            break;

        if ((value = IteratorNext(iter)) == nullptr) {
            Release(key);
            Release(iter);
            Release(map);
            return (Map *) ErrorFormat(type_value_error_, "map update require an iterable object of even length");
        }

        ok = MapInsert(map, key, value);
        Release(key);
        Release(value);

        if (!ok) {
            Release(iter);
            Release(map);
            return nullptr;
        }
    }

    Release(iter);
    return map;
}

void argon::object::MapClear(Map *map) {
    MapEntry *tmp;

    for (auto *cur = (MapEntry *) map->hmap.iter_begin; cur != nullptr; cur = tmp) {
        tmp = (MapEntry *) cur->iter_next;
        Release(cur->key);
        Release(cur->value);
        HMapRemove(&map->hmap, cur);
    }
}