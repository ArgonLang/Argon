// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/refcount.h>

using namespace argon::object;

TEST(RefCount, InlineCounter) {
    RefCount ref(ARGON_OBJECT_REFCOUNT_INLINE);
    ref.IncStrong();
    ASSERT_TRUE(ref.DecStrong());
}

TEST(RefCount, StaticResource) {
    RefCount ref(ARGON_OBJECT_REFCOUNT_STATIC);
    ref.IncStrong();
    ASSERT_FALSE(ref.DecStrong());
}