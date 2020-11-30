// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/arobject.h>
#include <object/datatype/string.h>
#include <object/datatype/map.h>

using namespace argon::object;

TEST(Map, AddItem) {
    ArObject *key;
    ArObject *value;

    argon::object::Map *map = MapNew();

    key = StringNew("h2o");
    value = StringNew("water");
    MapInsert(map, key, value);
    Release(key);
    Release(value);


    key = StringNew("ch4");
    value = StringNew("methane");
    MapInsert(map, key, value);
    Release(key);
    Release(value);

    key = StringNew("ch3");
    value = StringNew("methyl");
    MapInsert(map, key, value);

    ArObject *tmp = MapGet(map, key);
    ASSERT_TRUE(tmp->type->equal(tmp, value));
    Release(key);
    Release(value);

    key = StringNew("h2o");
    value = StringNew("water");
    tmp = MapGet(map, key);
    ASSERT_TRUE(tmp->type->equal(tmp, value));
    Release(key);
    Release(value);

    key = StringNew("ch4");
    value = StringNew("methane");
    tmp = MapGet(map, key);
    ASSERT_TRUE(tmp->type->equal(tmp, value));
    Release(key);
    Release(value);

    Release(map);
}

TEST(Map, RmItem) {
    ArObject *key;
    ArObject *value;

    argon::object::Map *map = MapNew();

    key = StringNew("h2o");
    value = StringNew("water");
    MapInsert(map, key, value);
    Release(key);
    Release(value);


    key = StringNew("ch4");
    value = StringNew("methane");
    MapInsert(map, key, value);
    Release(key);
    Release(value);

    key = StringNew("ch3");
    value = StringNew("methyl");
    MapInsert(map, key, value);

    ArObject *tmp = MapGet(map, key);
    ASSERT_TRUE(tmp->type->equal(tmp, value));
    Release(key);
    Release(value);

    key = StringNew("h2o");
    value = StringNew("water");
    tmp = MapGet(map, key);
    ASSERT_TRUE(tmp->type->equal(tmp, value));
    Release(key);
    Release(value);

    key = StringNew("ch4");
    MapRemove(map, key);
    ASSERT_TRUE(MapGet(map, key) == nullptr);
    Release(key);

    Release(map);
}