// This source file is part of the Stratum project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>
#include <codecvt>

#include <memutil.h>

using namespace stratum;

TEST(Memutil, MemoryCompare) {
    const char *p1 = "test123";

    ASSERT_EQ(util::MemoryCompare(p1, "test123", strlen(p1)), 0);
    ASSERT_EQ(util::MemoryCompare(p1, "", 0), 0);
    ASSERT_EQ(util::MemoryCompare("", p1, 0), 0);
    ASSERT_EQ(util::MemoryCompare(p1, "tert123", strlen(p1)), 's' - 'r');
}

TEST(Memutil, MemoryConcat) {
    const char *p1 = "test";
    const char *p2 = "stratum";
    char dst[11];

    util::MemoryConcat(dst, 11, p1, strlen(p1), p2, strlen(p2));
    ASSERT_EQ(util::MemoryCompare(dst, "teststratum", 11), 0);

    util::MemoryConcat(dst, 9, p1, strlen(p1), p2, strlen(p2));
    ASSERT_EQ(util::MemoryCompare(dst, "teststrat", 9), 0);

    util::MemoryConcat(dst, 3, p1, strlen(p1), p2, strlen(p2));
    ASSERT_EQ(util::MemoryCompare(dst, "tes", 3), 0);
}

TEST(Memutil, MemoryCopy) {
    const char *p1 = "Stratum";
    char dst[11];

    util::MemoryCopy(dst, p1, strlen(p1));
    ASSERT_EQ(util::MemoryCompare(dst, p1, strlen(p1)), 0);
}

TEST(Memutil, MemoryFind) {
    const char *p1 = "Stratum";

    ASSERT_EQ(util::MemoryFind(p1, 'S', strlen(p1)), p1);
    ASSERT_EQ(util::MemoryFind(p1, 'a', strlen(p1)), p1+3);
    ASSERT_EQ(util::MemoryFind(p1, 'J', strlen(p1)), nullptr);
}

TEST(Memutil, MemorySet) {
    char dst[8] = {"Stratum"};

    ASSERT_EQ(util::MemoryCompare(dst, "Stratum", 7), 0);

    util::MemorySet(dst, 'J', 8);

    ASSERT_EQ(util::MemoryCompare(dst, "JJJJJJJJ", 8), 0);
}