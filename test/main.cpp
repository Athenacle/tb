#include "gtest/gtest.h"

#include "tests.h"

void test::initTests() {}

int main(int argc, char* argv[])
{
    test::initTests();
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
