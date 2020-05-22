// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/object.h>
#include <object/refcount.h>
#include <object/list.h>
#include <object/nil.h>

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

TEST(RefCount, WeakObject) {
    auto list = ListNew();
    assert(list != nullptr);
    RefCount weak(list->ref_count.IncWeak());

    ArObject *tmp = weak.GetObject();
    ASSERT_EQ(tmp->type, &type_list_);
    Release(tmp);

    Release(list);

    tmp = weak.GetObject();
    ASSERT_EQ(tmp, NilVal);
    Release(tmp);
    weak.DecWeak();
}
