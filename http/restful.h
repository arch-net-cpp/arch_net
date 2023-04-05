#pragma once
#include "define.h"
#include "group.h"
#include "router.h"
#include "../common.h"

namespace arch_net { namespace coin {

class RestFul {

public:
    RestFul() { router_ = std::make_unique<Router>();}

    ~RestFul() = default;

protected:
    void handle(HttpRequest& req, HttpResponse& res) {
        Next done;
        done = [&](SideError& err) {
            if (!err.empty()) {
                std::string err_msg = "internal error! please check log. err: " + err.error_msg;
                LOG(ERROR) << err_msg;
                res.keep_alive(false);
                res.text(err.status_code, err_msg);
            }
        };
        handle(req, res, done);
    }
public:
    RestFul& use(const std::string& path, const Middleware& m) {
        router_->use(path, m);
        return *this;
    }

    RestFul& use(const std::string& path, const std::vector<Middleware>& ms) {
        router_->use(path, ms);
        return *this;
    }


    RestFul& use(RouteGroup& g) {
        if (!g.middle_wares().empty()) {
            router_->use(g.prefix_path(), g.middle_wares());
        }
        for (const auto& gmh : g.get_apis()) {
            if (gmh.path.empty()) {  // group root route
                router_->init_basic_method(gmh.method, g.prefix_path(), gmh.ms);
                continue;
            }
            std::string final_path;
            slim_path(g.prefix_path() + "/" + gmh.path, final_path);
            router_->init_basic_method(gmh.method, final_path, gmh.ms);
        }
        return *this;
    }

    RestFul& get(const std::string& path, const HttpHandler &m) {
        router_->get(path, m);
        return *this;
    }
    RestFul& get(const std::string& path, const std::vector<HttpHandler>& ms) {
        router_->get(path, ms);
        return *this;
    }
    RestFul& post(const std::string& path, const HttpHandler& m) {
        router_->post(path, m);
        return *this;
    }
    RestFul& post(const std::string& path, const std::vector<HttpHandler>& ms) {
        router_->post(path, ms);
        return *this;
    }
    RestFul& del(const std::string& path, const HttpHandler& m) {
        router_->del(path, m);
        return *this;
    }
    RestFul& del(const std::string& path, const std::vector<HttpHandler>& ms) {
        router_->del(path, ms);
        return *this;
    }
    RestFul& put(const std::string& path, const HttpHandler& m) {
        router_->put(path, m);
        return *this;
    }
    RestFul& put(const std::string& path, const std::vector<HttpHandler>& ms) {
        router_->put(path, ms);
        return *this;
    }


    // stat
    std::vector<std::pair<std::string, std::string>> stat() {
        std::vector<std::pair<std::string, std::string>> result;
        router_->trie_.walk_trie(result);
        return result;
    }

    std::string text_stat() {
        std::vector<std::pair<std::string, std::string>> result = stat();
        std::string content;
        for (auto& r : result) {
            content += r.first + "\t" + r.second;
            content += "\n";
        }
        return content;
    }

private:
    void handle(HttpRequest& req, HttpResponse& res, Next& done) {
        SideError err;
        try {
            router_->handle(req, res, err);
        } catch (const std::exception& e) {
            err.status_code = 500;
            err.error_msg = "[ERROR]" + std::string(e.what());
            LOG(ERROR) << err.error_msg;
        } catch (...) {
            err.status_code = 500;
            err.error_msg = "[ERROR] unknown process request!";
            LOG(ERROR) << err.error_msg;
        }
        done(err);
    }


private:
    std::unique_ptr<Router> router_;

};

}  }
