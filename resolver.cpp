
#include "resolver.h"

namespace arch_net {

std::unique_ptr<IOWorkerPool> ResolverWithLB::worker_ = std::make_unique<IOWorkerPool>(1);

DNSResolver::DNSResolver(const std::string& server_name)
    : ResolverWithLB("DNS resolver"), service_name_(server_name) {

    DNSResolver::resolve(resolve_cache_);
    load_balance_->on_changed(resolve_cache_);
}

void DNSResolver::get_servers(std::vector<ServerNode> &server_nodes) {
    if (!resolve_cache_.empty()) {
        server_nodes = resolve_cache_;
        return;
    }
    resolve(server_nodes);
}

ServerNode DNSResolver::get_next(uint32_t seed) {
    return load_balance_->get_next(seed, resolve_cache_);
}

void DNSResolver::resolve(std::vector<ServerNode> &server_nodes)  {
    std::vector<std::string> addrs;
    std::string host_name;
    int port;
    SplitHostPort(service_name_, host_name, port);
    dns_resolve(host_name, addrs);

    for(auto& addr : addrs) {
        server_nodes.emplace_back();
        server_nodes.back().endpoint.from(addr, port);
    }
    std::sort(server_nodes.begin(), server_nodes.end());
}

void DNSResolver::discard_cache()  {
    resolve_cache_.clear();
}

ServerNode FixedListResolver::get_next(uint32_t seed) {
    return load_balance_->get_next(seed, resolve_cache_);
}

}