#include "node.h"

namespace arch_net { namespace coin {


bool Node::check_method(const std::string& method) {
    if (method.empty()) return false;
    if (supported_http_methods.find(method) != supported_http_methods.end()) {
        return true;
    }
    return false;
}

Node* Node::find_child(const std::string& key) {
    for (auto& n : children) {
        if (std::strcmp(key.c_str(), n.key.c_str()) == 0) {
            return n.val;
        }
    }
    return nullptr;
}

bool Node::find_handler(const std::string& method) {
    auto found = handlers.find(method);
    if (found != handlers.end() && found->second.size() > 0) {
        return true;
    }
    return false;
}

Node* Node::_use(const Middleware& m) {
    std::string action_id = gen_random_str();
    ActionHolder action(action_id, this, m);
    middlewares.emplace_back(action);
    return this;
}

Node* Node::use(const Middleware& m) { return _use(m); }

Node* Node::use(const std::vector<Middleware>& ms) {
    for (auto& m : ms) {
        _use(m);
    }
    return this;
}


Node* Node::handle(const std::string& method, const Middleware& m) {
    std::ostringstream oss;
    if (!check_method(method)) {
        oss << "error method:" << method;
        throw Exception(oss.str());  // should not register unsupported method->handler.
    }

    if (find_handler(method)) {
        oss << "[" << pattern << "] [" << method << "] handler exists yet!!!";
        throw Exception(oss.str());
    }

    std::string action_id = gen_random_str();
    ActionHolder action(action_id, this, m);

    handlers[method].emplace_back(action);
    allow.insert(method);
    return this;
}

Node* Node::handle(const std::string& method, const std::vector<Middleware>& ms) {
    std::ostringstream oss;
    if (!check_method(method)) {
        oss << "error method:" << method;
        throw Exception(oss.str());  // should not register unsupported method->handler.
    }

    if (find_handler(method)) {
        oss << "[" << pattern << "] [" << method << "] handler exists yet!!!";
        throw Exception(oss.str());
    }

    for (auto& m : ms) {
        std::string action_id = gen_random_str();
        ActionHolder action(action_id, this, m);
        handlers[method].emplace_back(action);
    }

    allow.insert(method);
    return this;
}

}}
