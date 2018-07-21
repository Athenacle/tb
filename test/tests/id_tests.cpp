#include "gtest/gtest.h"

#include "id.h"
#include "tests.h"

using namespace only;

TEST(REGEX_TEST, charBarCodeValidate)
{
    EXPECT_TRUE(checkBarCodeValidate("11831G5010003"));
    EXPECT_TRUE(checkBarCodeValidate("1183AAAAA0003"));
    EXPECT_TRUE(checkBarCodeValidate("1183123450003"));
    EXPECT_FALSE(checkBarCodeValidate("4183123450003"));
    EXPECT_FALSE(checkBarCodeValidate("1383123450003"));
    EXPECT_FALSE(checkBarCodeValidate("1185123450003"));
    EXPECT_FALSE(checkBarCodeValidate("118512345000"));
    EXPECT_FALSE(checkBarCodeValidate("11851234500034"));
    EXPECT_FALSE(checkBarCodeValidate("1185123450A03"));
}

TEST(REGEX_TEST, restoreFullCode)
{
    std::string code;
    EXPECT_TRUE(restoreFullCode("118343526J19130", "1183435260012", code));
    EXPECT_EQ(code, "118343526J19130");

    EXPECT_TRUE(restoreFullCode("118343000J19130", "1183430000012", code));
    EXPECT_EQ(code, "118343000J19130");

    EXPECT_FALSE(restoreFullCode("11834300J19130", "12345678999999", code));
}

TEST(REGEX_TEST, price)
{
    EXPECT_EQ(checkPrice("￥299"), 299);
    EXPECT_EQ(checkPrice("\uffe5299"), 299);

    EXPECT_TRUE(checkFullCode("11834951519131"));
}
