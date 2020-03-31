// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/map.h>
#include <object/nil.h>
#include <object/string.h>

TEST(Map, AddItem) {
    argon::object::ObjectContainer key;
    argon::object::ObjectContainer value;
    argon::object::Map map;

    key = argon::object::MakeOwner<argon::object::String>("h2o");
    value = argon::object::MakeOwner<argon::object::String>("water");
    map.Insert(key.Get(), value.Get());

    key = argon::object::MakeOwner<argon::object::String>("ch4");
    value = argon::object::MakeOwner<argon::object::String>("methane");
    map.Insert(key.Get(), value.Get());

    key = argon::object::MakeOwner<argon::object::String>("ch3");
    value = argon::object::MakeOwner<argon::object::String>("methyl");
    map.Insert(key.Get(), value.Get());

    ASSERT_TRUE(map.GetItem(key.Get())->EqualTo(value.Get()));
    key = argon::object::MakeOwner<argon::object::String>("h2o");
    value = argon::object::MakeOwner<argon::object::String>("water");
    ASSERT_TRUE(map.GetItem(key.Get())->EqualTo(value.Get()));
}

TEST(Map, RmItem) {
    argon::object::ObjectContainer key;
    argon::object::ObjectContainer value;
    argon::object::Map map;

    key = argon::object::MakeOwner<argon::object::String>("h2o");
    value = argon::object::MakeOwner<argon::object::String>("water");
    map.Insert(key.Get(), value.Get());

    key = argon::object::MakeOwner<argon::object::String>("ch4");
    value = argon::object::MakeOwner<argon::object::String>("methane");
    map.Insert(key.Get(), value.Get());

    key = argon::object::MakeOwner<argon::object::String>("ch3");
    value = argon::object::MakeOwner<argon::object::String>("methyl");
    map.Insert(key.Get(), value.Get());

    ASSERT_TRUE(map.GetItem(key.Get())->EqualTo(value.Get()));
    key = argon::object::MakeOwner<argon::object::String>("ch4");
    map.Remove(key.Get());
    ASSERT_TRUE(map.GetItem(key.Get())->EqualTo(argon::object::Nil::NilValue()));
}