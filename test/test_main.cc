#include <gtest/gtest.h>
#include <vm/argon.h>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    argon::vm::Initialize();
    return RUN_ALL_TESTS();
}