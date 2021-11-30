// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <gtest/gtest.h>

#include <object/arobject.h>
#include <object/refcount.h>
#include <object/datatype/list.h>
#include <object/datatype/nil.h>

using namespace argon::object;

TEST(RefCount, InlineCounter) {
    RefCount ref(RCType::INLINE);
    ASSERT_TRUE(ref.DecStrong());
}

TEST(RefCount, StaticResource) {
    RefCount ref(RCType::STATIC);
    ASSERT_FALSE(ref.DecStrong());
}

TEST(RefCount, GCResource) {
    RefCount ref(RCType::GC);
    ASSERT_TRUE(ref.DecStrong());
}

TEST(RefCount, WeakInc) {
    RefCount strong(RCType::INLINE);
    RefCount weak(strong.IncWeak());
    ASSERT_TRUE(strong.DecStrong());
    ASSERT_TRUE(weak.DecWeak());
}

TEST(RefCount, WeakGCInc) {
    RefCount strong(RCType::GC);
    RefCount weak(strong.IncWeak());
    ASSERT_TRUE(strong.DecStrong());
    ASSERT_TRUE(weak.DecWeak());
}

TEST(RefCount, WeakObject) {
    auto list = ListNew();
    assert(list != nullptr);
    RefCount weak(list->ref_count.IncWeak());

    ArObject *tmp = weak.GetObject();
    ASSERT_EQ(tmp->type, type_list_);
    Release(tmp);

    Release(list);

    tmp = ReturnNil(weak.GetObject());
    ASSERT_EQ(tmp, NilVal);
    Release(tmp);
    weak.DecWeak();
}
