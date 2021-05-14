// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/gc.h>
#include <object/arobject.h>
#include <object/datatype/list.h>

using namespace argon::object;

TEST(GC, SimpleTrack) {
    auto l1 = ListNew();
    auto l2 = ListNew();

    Collect();

    GCStats stat = GetStats(0);
    ASSERT_EQ(stat.count, 2);
    ASSERT_EQ(stat.collected, 0);
    ASSERT_EQ(stat.uncollected, 2);

    Release(l1);
    Release(l2);
}

TEST(GC, SelfRecursive) {
    auto l1 = ListNew();

    ListAppend(l1, l1);

    Release(l1);

    Collect();

    GCStats stat = GetStats(0);
    ASSERT_EQ(stat.count, 1);
    ASSERT_EQ(stat.collected, 1);
    ASSERT_EQ(stat.uncollected, 0);
}

TEST(GC, SelfRecursiveWithObjNoRef) {
    auto l1 = ListNew();
    auto l2 = ListNew();

    ListAppend(l1, l1);
    ListAppend(l1, l2);

    Release(l1);
    Release(l2);

    Collect();

    GCStats stat = GetStats(0);
    ASSERT_EQ(stat.count, 2);
    ASSERT_EQ(stat.collected, 2);
    ASSERT_EQ(stat.uncollected, 0);
}

TEST(GC, SelfRecursiveWithRefToRootObj) {
    auto l1 = ListNew();
    auto l2 = ListNew();

    ListAppend(l1, l2);
    ListAppend(l1, l1);

    Release(l1);

    Collect();

    Release(l2);

    GCStats stat = GetStats(0);
    ASSERT_EQ(stat.count, 2);
    ASSERT_EQ(stat.collected, 1);
    ASSERT_EQ(stat.uncollected, 1);
}

TEST(GC, Recursive) {
    auto l1 = ListNew();
    auto l2 = ListNew();
    auto l3 = ListNew();

    ListAppend(l1, l2);
    ListAppend(l2, l1);
    ListAppend(l1, l3);
    ListAppend(l2, l3);

    Release(l1);
    Release(l2);

    Collect();

    Release(l3);

    GCStats stat = GetStats(0);
    ASSERT_EQ(stat.count, 3);
    ASSERT_EQ(stat.collected, 2);
    ASSERT_EQ(stat.uncollected, 1);
}
