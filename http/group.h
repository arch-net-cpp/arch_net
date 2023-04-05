#pragma once
#include "define.h"
#include "../common.h"

namespace arch_net { namespace coin {

struct RouteGroupMethodHolder {
    std::string method;
    std::string path;
    std::vector<HttpHandler> ms;

    RouteGroupMethodHolder(const std::string& m, const std::string& p,
                           const std::vector<HttpHandler>& middlewares)
                           : method(m), path(p), ms(middlewares) {}
};

// a simple wrapper for related routes
class RouteGroup {
public:
    RouteGroup(const std::string& prefix_path, const std::vector<Middleware>& ms = {})
        : name_("DEFAULT_CIAO_GROUP"), prefix_path_(prefix_path),middle_ware_apis_(ms) {}

    ~RouteGroup() {}

    const std::string& name() const { return name_; }
    const std::vector<RouteGroupMethodHolder>& get_apis() const { return _apis; }
    const std::vector<Middleware>& middle_wares() const { return middle_ware_apis_; }

    const std::string& prefix_path() const { return prefix_path_; }

    // 挂载到path指定的group子路由上，不使用模板和宏，显式定义各个方法
    RouteGroup& get(const std::string& path, const HttpHandler& m) {
        _set_api("GET", path, {m});
        return *this;
    }
    RouteGroup& get(const std::string& path, const std::vector<HttpHandler>& ms) {
        _set_api("GET", path, ms);
        return *this;
    }
    RouteGroup& post(const std::string& path, const HttpHandler& m) {
        _set_api("POST", path, {m});
        return *this;
    }
    RouteGroup& post(const std::string& path, const std::vector<HttpHandler>& ms) {
        _set_api("POST", path, ms);
        return *this;
    }
    RouteGroup& del(const std::string& path, const HttpHandler& m) {
        _set_api("DELETE", path, {m});
        return *this;
    }
    RouteGroup& del(const std::string& path, const std::vector<HttpHandler>& ms) {
        _set_api("DELETE", path, ms);
        return *this;
    }
    RouteGroup& put(const std::string& path, const HttpHandler& m) {
        _set_api("PUT", path, {m});
        return *this;
    }
    RouteGroup& put(const std::string& path, const std::vector<HttpHandler>& ms) {
        _set_api("PUT", path, ms);
        return *this;
    }

private:
    bool _check_method(const std::string& method) {
        if (method.empty()) return false;
        if (supported_http_methods.find(method) != supported_http_methods.end()) {
            return true;
        }
        return false;
    }

    void _set_api(const std::string& method, const std::string& path, const std::vector<HttpHandler>& ms) {
        if (!_check_method(method)) {
            std::string err_msg = "method:" + method + " is not supported!";
            LOG(ERROR) << err_msg;
            throw Exception(err_msg);
        }
        _apis.emplace_back(method, path, ms);
    }

private:
    std::string name_;
    std::string prefix_path_;
    std::vector<RouteGroupMethodHolder> _apis;
    std::vector<Middleware> middle_ware_apis_;

};

}}
