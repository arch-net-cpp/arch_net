#pragma once
#include "common.h"
#include "map"
#include "random"
#include "socket_stream.h"

namespace arch_net {

enum class LoadBalanceType {
    ConsistentHash,
    Random,
    WeightedRandom
};


struct ServerNode {
    EndPoint endpoint{};
    std::string tag{};
    int32_t weight{-1};
    bool valid{false};

    friend inline bool operator<(const ServerNode& s1, const ServerNode& s2) {
        return s1.endpoint == s2.endpoint ? s1.weight < s2.weight : s1.endpoint < s2.endpoint;
    }
};

class LoadBalance {
public:
    virtual ServerNode get_next(uint32_t seed, const std::vector<ServerNode>& servers = {}) = 0;
    virtual ~LoadBalance() {}
    virtual void on_changed(const std::vector<ServerNode>& new_servers) = 0;
};

class ConsistentHashBalance : public LoadBalance {
public:
    using ServerRing = std::map<uint32_t, ServerNode>;

    ConsistentHashBalance(int factor) : factor_(factor){}

    ~ConsistentHashBalance() = default;

    ServerNode get_next(uint32_t seed, const std::vector<ServerNode>& servers = {}) override{
        if (!servers.empty() && ring_.empty()) {
            ServerRing ring;
            initialize_circle(servers, ring);
            mutex_.lock();
            ring_ = std::move(ring);
            mutex_.unlock();
        }
        mutex_.lock();
        defer(mutex_.unlock());
        //auto a = --ring_.end();
        //seed = seed % a->first;
        auto node = ring_.lower_bound(seed);
        if (node == ring_.end()) {
            return ring_.begin()->second;
        }
        return node->second;
    }

    void on_changed(const std::vector<ServerNode> &new_servers) override {
        ServerRing ring;
        initialize_circle(new_servers, ring);
        ring_ = std::move(ring);
    }

private:
    void initialize_circle(const std::vector<ServerNode>& servers, ServerRing& ring) const {
        for (auto& server : servers) {
            for (size_t i = 0; i < factor_; i++) {
                std::string key = arch_net::string_printf(
                        "%s:%d-%zu", server.endpoint.host.c_str(), server.endpoint.port, i);
                uint32_t val[4];
                MurmurHash3_x64_128(key.c_str(), key.size(), 0, val);
                // hash产生4个值，实际上是[factor_*4]个虚节点
                for (size_t j = 0; j < 4; j++) {
                    ring[val[j]] = server;
                }
            }
        }
    }

private:
    size_t size_;
    size_t factor_;
    ServerRing ring_;
    acl::fiber_mutex mutex_;
};

class RandomBalance : public LoadBalance {
public:
    RandomBalance() : gen_(std::random_device{}()) {}
    ServerNode get_next(uint32_t seed, const std::vector<ServerNode>& servers = {}) override {
        if (servers.size() == 1) {
            return servers[0];
        }
        if (seed != 0) {
            gen_.seed(seed);
        }
        std::uniform_int_distribution<int> dis(0, servers.size()-1);
        int x = dis(gen_);
        return servers[x];
    }

    void on_changed(const std::vector<ServerNode> &new_servers) override {}

private:
    std::mt19937 gen_;
};

class WeightedRandomBalance : public LoadBalance {
public:
    WeightedRandomBalance() : gen_(std::random_device{}()) {}

    ServerNode get_next(uint32_t seed, const std::vector<ServerNode>& servers = {}) override {
        if (seed != 0) {
            gen_.seed(seed);
        }
        std::vector<int> presum(servers.size());
        int total = 0;
        for (int i = 0; i < servers.size(); i++) {
            total += servers[i].weight;
            if (i > 0) {
                presum[i] = presum[i - 1] + servers[i].weight;
            } else {
                presum[i] = servers[i].weight;
            }
        }
        std::uniform_int_distribution<int> dis(1, total);
        int x = dis(gen_);
        int idx = binarySearch(presum, x);
        return servers[idx];
    }

    void on_changed(const std::vector<ServerNode> &new_servers) override {}

    static int binarySearch(const std::vector<int>& presum, int x) {
        int low = 0, high = presum.size() - 1;
        while (low < high) {
            int mid = (high - low) / 2 + low;
            if (presum[mid] < x) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        return low;
    }
private:
    std::mt19937 gen_;
};


static LoadBalance* new_load_balance(LoadBalanceType type) {
    switch (type) {
    case LoadBalanceType::ConsistentHash:
        return new ConsistentHashBalance(20);
    case LoadBalanceType::Random:
        return new RandomBalance();
    case LoadBalanceType::WeightedRandom:
        return new WeightedRandomBalance();
    }
    return new RandomBalance();
}

}