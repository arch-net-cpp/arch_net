#pragma once
#include <inttypes.h>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <glog/logging.h>

namespace arch_net { namespace coin {

extern std::unordered_map<std::string, bool> supported_http_methods;

class HttpRequest;
class HttpResponse;

struct SideError {
    int status_code{-1};
    std::string error_msg;
    bool empty() const {
        return status_code == -1 && error_msg.empty();
    }
};

// err不为空则出错
typedef std::function<void(SideError& err)> Next;
typedef std::function<void(HttpRequest& req, HttpResponse& res, SideError& err)> Middleware;
typedef std::function<void(HttpRequest& req, HttpResponse& res)> HttpHandler;

}}  // namespace ciao
