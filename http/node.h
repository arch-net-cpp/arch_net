#pragma once
#include "define.h"
#include "../common.h"

namespace arch_net { namespace coin {

class Node;

struct NodeHolder {
    std::string key;
    Node* val;
};

struct ActionHolder {
    std::string id;
    Node* node;
    Middleware func;

    ActionHolder(std::string& i, Node* n, const Middleware& m)
        : id(std::move(i)), node(n), func(m) {}
};


struct Matched {
    Node* node;
    std::unordered_map<std::string, std::string> params;
    std::vector<Node*> pipeline;
    bool error{false};
};

class Node {
private:
    Node* _use(const Middleware& m);

public:
    std::string id;
    bool is_root;
    std::string name;
    std::unordered_set<std::string> allow;
    std::string pattern;
    bool endpoint;
    Node* parent;
    Node* colon_parent;
    std::vector<NodeHolder> children;
    Node* colon_child;
    std::unordered_map<std::string, std::vector<ActionHolder>> handlers;
    std::vector<ActionHolder> middlewares;
    std::string regex;

public:
    explicit Node(bool root = false) {
        is_root = root;
        parent = nullptr;
        colon_parent = nullptr;
        colon_child = nullptr;
        endpoint = false;
        if (is_root) {
            id = "root_" + gen_random_str();
        } else {
            id = "node_" + gen_random_str();
        }
    }

    Node(bool root, int unique_id) {
        is_root = root;
        parent = nullptr;
        colon_parent = nullptr;
        colon_child = nullptr;
        endpoint = false;
        if (is_root) {
            id = "root_" + std::to_string(unique_id);
        } else {
            std::ostringstream oss;
            oss << "node_" << gen_random_str() << "_" << unique_id;
            id = oss.str();
        }
    }

    ~Node() {
        // TODO: check
        // std::cout << "release node: " << id << std::endl;
        // if (colon_child) delete colon_child;
        // for (auto& nh : children) {
        //    if (nh.val) delete nh.val;
        //}
    }

    bool check_method(const std::string& method);
    Node* find_child(const std::string& key);
    bool find_handler(const std::string& method);
    Node* use(const Middleware& m);
    Node* use(const std::vector<Middleware>& ms);
    Node* handle(const std::string& method, const Middleware& m);
    Node* handle(const std::string& method, const std::vector<Middleware>& ms);
};

typedef std::shared_ptr<Node> NodePtr;

}}  // namespace ciao
