#include "router.h"

namespace arch_net { namespace coin {

Router *Router::use(const std::string &path, const Middleware &m) {
    Node* node;
    if (path.empty()) {
        node = trie_.root.get();
    } else {
        node = trie_.add_node(path);
    }
    node->use(m);
    return this;
}

Router *Router::use(const std::string &path, const std::vector<Middleware> &ms) {
    Node* node;
    if (path.empty()) {
        node = trie_.root.get();
    } else {
        node = trie_.add_node(path);
    }
    node->use(ms);
    return this;
}

void Router::handle(HttpRequest &req, HttpResponse &res, SideError &err) {
    const std::string& path = req.url_path();
    const std::string& method = req.get_method();
    if (method.empty()) {
        err.status_code = 404;
        err.error_msg = "404! not found.";
        return;
    }
    Matched matched;
    trie_.match(path, matched);
    if (matched.error) {
        err.status_code = 500;
        err.error_msg = "Internal error.";
        return;
    }

    Node* matched_node = matched.node;
    if (!matched_node) {
        err.status_code = 404;
        err.error_msg = "404! not found.";
        return;
    }

    auto it = matched_node->handlers.find(method);
    if (it == matched_node->handlers.end() || it->second.empty()) {
        err.status_code = 404;
        err.error_msg = "404! not found.";
        return;
    }

    std::vector<ActionHolder> stack;  // an array actually, not a real `stack`
    compose_func(matched, method, stack);
    if (stack.empty()) {
        err.status_code = 404;
        err.error_msg = "404! not found.";
        return;
    }

    std::unordered_map<std::string, std::string>& parsed_params = matched.params;
    req.params = parsed_params;

    for (auto & handler : stack) {
        try {
            handler.func(req, res, err);
        } catch (const std::exception& e) {
            err.status_code = 500;
            err.error_msg = "[HANDLER ERROR]" + std::string(e.what());
            LOG(ERROR) << err.error_msg;
        } catch (...) {
            err.status_code = 500;
            err.error_msg = "[HANDLER ERROR] unknown process request!";
            LOG(ERROR) << err.error_msg;
        }
        if (!err.empty()) {
            return;
        }
    }
}

void Router::compose_func(Matched &matched, const std::string &method,
                          std::vector<ActionHolder> &stack) {
    Node* exact_node = matched.node;
    if (!exact_node) {
        return;
    }

    std::vector<Node*>& pipeline = matched.pipeline;
    if (pipeline.empty()) {
        return;
    }

    for (size_t i = 0; i < pipeline.size(); i++) {
        Node* p = pipeline[i];
        std::vector<ActionHolder>& middlewares = p->middlewares;
        for (const ActionHolder& ah : middlewares) {
            stack.push_back(ah);
        }

        std::unordered_map<std::string, std::vector<ActionHolder>>& handlers = p->handlers;

        // TODO: id must be unique!
        if (p->id == exact_node->id && handlers.find(method) != handlers.end()) {
            std::vector<ActionHolder>& hs = handlers[method];
            for (ActionHolder& ah : hs) {
                stack.push_back(ah);
            }
        }
    }
}

Router *Router::init_basic_method(const std::string &method, const std::string &path,
                                  const std::vector<HttpHandler> &hs) {
    Node* node = trie_.add_node(path);
    std::vector<Middleware> ms;
    for (auto & h : hs) {
        ms.emplace_back([=](HttpRequest& req, HttpResponse& res, SideError& err){
            h(req, res);
        });
    }
    node->handle(method, ms);
    return this;
}

}}