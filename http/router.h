#pragma once
#include "define.h"
#include "group.h"
#include "node.h"
#include "request.h"
#include "response.h"
#include "trie.h"
#include "../common.h"

namespace arch_net { namespace coin {

class Router {
public:
    Router(const std::string& n = "", const std::string& segment_pattern = "[A-Za-z0-9\\._~\\-]+")
        : name(n),trie_() {

        if (n.empty()) name = "router-" + gen_random_str();
    }
    //  Router() returns `Middleware`, so router itself could be regarded as Middleware.
//    Middleware operator()() { return call(); }
//
//    Middleware call() {
//        return [this](HttpRequest& req, HttpResponse& res, Next& next) {
//            handle(req, res, next);
//            return;
//        };
//    }

    void handle(HttpRequest& req, HttpResponse& res, SideError& err);

    Router* init_basic_method(const std::string& method, const std::string& path,
                              const std::vector<HttpHandler>& ms);

    Router* use(const std::string& path, const Middleware& m);

    Router* use(const std::string& path, const std::vector<Middleware>& ms);

    Router* get(const std::string& path, const HttpHandler& m) {
        return init_basic_method("GET", path, {m});
    }
    Router* get(const std::string& path, const std::vector<HttpHandler>& ms) {
        return init_basic_method("GET", path, ms);
    }
    Router* post(const std::string& path, const HttpHandler& m) {
        return init_basic_method("POST", path, {m});
    }
    Router* post(const std::string& path, const std::vector<HttpHandler>& ms) {
        return init_basic_method("POST", path, ms);
    }
    Router* del(const std::string& path, const HttpHandler& m) {
        return init_basic_method("DELETE", path, {m});
    }
    Router* del(const std::string& path, const std::vector<HttpHandler>& ms) {
        return init_basic_method("DELETE", path, ms);
    }
    Router* put(const std::string& path, const HttpHandler& m) {
        return init_basic_method("PUT", path, {m});
    }
    Router* put(const std::string& path, const std::vector<HttpHandler>& ms) {
        return init_basic_method("PUT", path, ms);
    }
    // basic methods end.

private:
    void compose_func(Matched& matched, const std::string& method,
                      std::vector<ActionHolder>& stack);

public:
    std::string name;
    Trie trie_;
};

}}
