#include "gtest/gtest.h"
#include "../http/router/router.h"

using namespace arch_net::coin::router;

TEST(Test_Router, Test_match)
{
    Router router;
    {
        auto requestUrl = "/xxx/1/yyy/2";
        std::unordered_map<std::string, std::string> map;
        auto ret = router.match_and_parse(requestUrl, "/xxx/:param1/yyy/:param2", map);
        EXPECT_EQ(true, ret);
        EXPECT_EQ("1", map["param1"]);
        EXPECT_EQ("2", map["param2"]);
    }
    {
        auto requestUrl = "#xxx#1#yyy#2";
        std::unordered_map<std::string, std::string> map;
        auto ret = router.match_and_parse(requestUrl, "/xxx/:param1/yyy/:param2", map);
        EXPECT_EQ(false, ret);
    }
    {
        auto requestUrl = "/xxx/1/yyy/###";
        std::unordered_map<std::string, std::string> map;
        auto ret = router.match_and_parse(requestUrl, "/xxx/:param1/yyy/:param2", map);
        EXPECT_EQ(false, ret);
    }
}