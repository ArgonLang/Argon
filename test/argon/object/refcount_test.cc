// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/refcount.h>

using namespace argon::object;

TEST(RefCount, InlineCounter) {
    RefCount ref(ARGON_OBJECT_REFCOUNT_INLINE);
    ASSERT_TRUE(ref.DecStrong());
}

TEST(RefCount, StaticResource) {
    RefCount ref(ARGON_OBJECT_REFCOUNT_STATIC);
    ASSERT_FALSE(ref.DecStrong());
}

TEST(RefCount, WeakInc) {
    RefCount strong(ARGON_OBJECT_REFCOUNT_INLINE);
    RefCount weak(strong.IncWeak());
    ASSERT_TRUE(strong.DecStrong());
    ASSERT_TRUE(weak.DecWeak());
}
