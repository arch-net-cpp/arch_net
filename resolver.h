#pragma once
#include "socket_stream.h"
#include "common.h"
#include "load_balance.h"
namespace arch_net {

enum ResolveType {
    DNS,
    FixedList,

};

class ResolverWithLB {
public:
    ResolverWithLB(const std::string& name, LoadBalance* lb = new RandomBalance())
        : name_(name), load_balance_(lb) {
        ResolverWithLB::worker_->addTask([&](){
            run();
        });
    }
    virtual ~ResolverWithLB() {}

    virtual void get_servers(std::vector<ServerNode>& endpoints) = 0;

    virtual ServerNode get_next(uint32_t seed) = 0;

    void with_load_balance(LoadBalance* lb) { load_balance_.reset(lb);}

protected:
    virtual void resolve(std::vector<ServerNode>& endpoints) = 0;
    virtual void run() = 0;
    virtual void discard_cache() = 0;

    static std::unique_ptr<IOWorkerPool> worker_;

protected:
    std::string name_;
    std::unique_ptr<LoadBalance> load_balance_;
    bool exit_{false};
};

class DNSResolver : public ResolverWithLB {
public:
    DNSResolver(const std::string& server_name);

    ~DNSResolver() { exit_ = true;}

    void get_servers(std::vector<ServerNode> &server_nodes) override;

    ServerNode get_next(uint32_t seed = 0) override;

    void resolve(std::vector<ServerNode> &server_nodes) override ;

    void run() override {}

    void discard_cache() override;

private:
    std::vector<ServerNode> resolve_cache_;
    std::string service_name_;
};

class FixedListResolver : public ResolverWithLB {
public:
    FixedListResolver(const std::vector<std::pair<std::string, int>>& servers) : ResolverWithLB("list resolver") {
        for (auto & server : servers) {
            resolve_cache_.emplace_back();
            resolve_cache_.back().endpoint.from(server.first, server.second);
        }
    }
    void get_servers(std::vector<ServerNode> &endpoints) override {
        endpoints = resolve_cache_;
    }

    ServerNode get_next(uint32_t seed = 0) override;

    void resolve(std::vector<ServerNode> &endpoints) override {}

    void run() override {}

    void discard_cache() override { resolve_cache_.clear(); }
private:
    std::vector<ServerNode> resolve_cache_;
};

}