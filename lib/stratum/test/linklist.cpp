// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <support/linklist.h>

using namespace stratum;

struct TestObj {
    TestObj *next;
    TestObj **prev;

    unsigned int free;
};

using LinkList = support::LinkedList<TestObj>;

TEST(LinkList, InsertPop) {
    LinkList list;

    TestObj obj1{nullptr, nullptr, 1};
    TestObj obj2{nullptr, nullptr, 3};
    TestObj obj3{nullptr, nullptr, 5};


    list.Insert(&obj1);
    list.Insert(&obj2);
    list.Insert(&obj3);

    ASSERT_EQ(list.Count(), 3);
    ASSERT_EQ(list.Pop(), &obj3);
    ASSERT_EQ(list.Pop(), &obj2);
    ASSERT_EQ(list.Pop(), &obj1);
    ASSERT_EQ(list.Count(), 0);
}

TEST(LinkList, Sort) {
    LinkList list;

    TestObj obj1{nullptr, nullptr, 1};
    TestObj obj2{nullptr, nullptr, 5};
    TestObj obj3{nullptr, nullptr, 4};
    TestObj obj4{nullptr, nullptr, 6};

    list.Insert(&obj1);
    list.Insert(&obj2);
    list.Insert(&obj3);
    list.Insert(&obj4);

    list.Sort(&obj1);
    list.Sort(&obj4);
    list.Sort(&obj3);
    list.Sort(&obj4);

    ASSERT_EQ(list.FindFree(), &obj1);
    obj1.free--;

    ASSERT_EQ(list.FindFree(), &obj3);
    obj3.free-=4;

    ASSERT_EQ(list.FindFree(), &obj2);

    ASSERT_EQ(list.Pop(), &obj1);
    ASSERT_EQ(list.Pop(), &obj3);
    ASSERT_EQ(list.Pop(), &obj2);
    ASSERT_EQ(list.Pop(), &obj4);
}
