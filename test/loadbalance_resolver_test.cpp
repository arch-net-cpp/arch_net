
#include <gtest/gtest.h>
#include "../load_balance.h"

TEST(Test_LB_RV, test_RandomLB)
{
    auto random_lb = std::make_unique<arch_net::RandomBalance>();
    std::vector<arch_net::ServerNode> servers;
    for (int i = 0; i < 10; i++) {
        servers.emplace_back();
        servers.back().tag = arch_net::string_printf("%d", i);
    }

    std::unordered_map<std::string, int> count;

    for (int i = 0; i < 10000; i++) {
        auto tag = random_lb->get_next(0, servers).tag;
        count[tag]++;
    }
    for (auto& c : count) {
        std::cout << c.first << " " << c.second << std::endl;
    }
}


TEST(Test_LB_RV, test_Weight_RandomLB)
{
    auto random_lb = std::make_unique<arch_net::WeightedRandomBalance>();
    std::vector<arch_net::ServerNode> servers;
    for (int i = 0; i < 10; i++) {
        servers.emplace_back();
        servers.back().tag = arch_net::string_printf("%d", i);
        servers.back().weight = 100;
    }

    servers[4].weight = 50;

    servers[8].weight = 50;

    std::unordered_map<std::string, int> count;

    for (int i = 0; i < 10000; i++) {
        auto tag = random_lb->get_next(0, servers).tag;
        count[tag]++;
    }
    for (auto& c : count) {
        std::cout << c.first << " " << c.second << std::endl;
    }
}



TEST(Test_LB_RV, test_Consistent_RandomLB)
{
    auto qq = 289 % 3;
    auto random_lb = std::make_unique<arch_net::ConsistentHashBalance>(50);
    std::vector<arch_net::ServerNode> servers;
    for (int i = 0; i < 10; i++) {
        servers.emplace_back();
        servers.back().endpoint.host = "127.0.0.1";
        servers.back().endpoint.port = 8000 + i;
        servers.back().tag = arch_net::string_printf("%d", i);
        servers.back().weight = 100;
    }

    std::unordered_map<std::string, int> count;

    for (int i = 0; i < 100000; i++) {
        auto key = arch_net::string_printf("12233421441test_key_%d", i);
        uint32_t hash_val;
        MurmurHash3_x86_32(key.c_str(), key.size(), 0, &hash_val);

        auto tag = random_lb->get_next(hash_val, servers).tag;
        count[tag]++;
    }
    for (auto& c : count) {
        std::cout << c.first << " " << c.second << std::endl;
    }
}