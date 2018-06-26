#include "gtest/gtest.h"

#include "settings.h"
#include "tests.h"

using namespace tb::settings;

namespace
{
    auto jsonTEST_1 = "/test/json/test_1.json";
};


#define TEST_MACRO(path, status, TYPE)                \
    do {                                              \
        ASSERT_EQ(Settings::Read(path, val), status); \
        ASSERT_EQ(ValueType::TYPE, val.type);         \
    } while (false);


TEST(SHARED_Settings, JSONtest)
{
    auto path = new char[2 + strlen(PROJECT_BUILD_PATH) + strlen(jsonTEST_1)];
    strcpy(path, PROJECT_BUILD_PATH);
    strcat(path, jsonTEST_1);
    Settings::InitSettings(path);
    Value val;

    TEST_MACRO("/name", true, String);
    ASSERT_STREQ(val.String, "fruit");


    TEST_MACRO("/count", true, Integer);
    ASSERT_EQ(val.Integer, 3);

    TEST_MACRO("/apple/price", true, Double);
    ASSERT_EQ(1.5, val.Double);

    TEST_MACRO("/apple/count", true, Integer);
    ASSERT_EQ(10, val.Integer);

    TEST_MACRO("/apple/note", true, String);
    ASSERT_STREQ(val.String, "note");

    TEST_MACRO("/0", true, Integer);
    ASSERT_EQ(val.Integer, 100);

    TEST_MACRO("/appkey", true, String);
    ASSERT_STREQ(val.String, "01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ");

    delete[] path;
    Settings::DeInitSettings();
}
