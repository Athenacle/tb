#include "gtest/gtest.h"

#include <string>
#include "taobao.h"

using std::string;
using tb::utils::formatDirectoryPath;

#define TT(input, expect)       \
    do {                        \
        t = input;              \
        formatDirectoryPath(t); \
        EXPECT_EQ(t, expect);   \
    } while (false);

TEST(UTILS, formatDirectory)
{
    string t;
    TT("  apple", "apple");
    TT("./apple", "apple");
    TT(" ./apple//apple", "apple/apple");
}
