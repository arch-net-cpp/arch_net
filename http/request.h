#pragma once

#include "../buffer.h"
#include "http_parser.h"
#include <map>
namespace arch_net { namespace coin {

class HttpRequest {
public:

    HttpRequest();

    HttpRequest(HttpRequest & hr) { swap(hr); }

    HttpRequest(HttpRequest&& hr) { swap(hr);}

    HttpRequest(const HttpRequest & hr) = delete;

    int recv_and_parse(ISocketStream* stream);

    bool keep_alive() const {return is_keep_alive_; }

    inline bool completed() const { return is_completed; }

    std::string url_path() const;

    std::string url_query() const;

    std::string url_fragment() const;

    std::string url_userinfo() const;

    std::string get_header(const std::string& key) const;

    std::string get_query(const std::string& key) const;

    std::string get_params(const std::string& key) const;

    std::string get_method() const { return method_; }

    int parse(Buffer& buffer);

    bool upgrade_websocket() const { return websocket_upgrade_ && !websocket_key_.empty(); }

    std::string websocket_key() const { return websocket_key_; }

    void reset();

public:
    std::string body;
    std::map<std::string, std::string> field_value;
    http_parser parser;
    std::string remote_ip;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> query;

private:


    void swap(HttpRequest & hr);

    void parse_query(const std::string& str);

    void set_remote_ip(const std::string& ip) { remote_ip.assign(ip); }

    inline bool is_send_continue() const { return send_continue_; }

    inline void set_continue() { send_continue_ = true;}

    static int OnMessageBegin(http_parser *p) {
        return 0;
    }

    static int OnMessageEnd(http_parser *p) {
        auto req = static_cast<HttpRequest *>(p->data);
        req->is_completed = true;
        http_parser_pause(p, 1);

        if (p->http_major > 9 || p->http_minor > 9) {
            p->http_major = 1;
            p->http_minor = 1;
        }

        if (p->http_major == 1) {
            if (p->http_minor == 1) {req->is_keep_alive_ = true;}
            if (strcasecmp(req->get_header("Connection").c_str(), "close" ) == 0 ) {
                req->is_keep_alive_ = false;
            }
        }

        req->method_ = http_method_str( (http_method) p->method);

        return 0;
    }

    static int OnUrl(http_parser *p, const char *buf, size_t len) {
        auto req = static_cast<HttpRequest *>(p->data);
        req->url.append(buf, len);
        return 0;
    }

    static int OnField(http_parser *p, const char *buf, size_t len) {
        auto req = static_cast<HttpRequest *>(p->data);
        if (req->pre_state == 51) {
            req->field.append(buf, len);
        } else {
            req->field.assign(buf, len);
        }
        req->pre_state = p->state;
        return 0;
    }

    static int OnValue(http_parser *p, const char *buf, size_t len) {
        auto req = static_cast<HttpRequest *>(p->data);
        if (req->pre_state == 53/**/) {
            req->value.append(buf, len);
        } else {
            req->value.assign(buf, len);
        }

        unsigned char state = p->state;
        if (state == 55/*head value*/) {
            req->field_value[req->field] = std::move(req->value);
            req->field.clear();
        }
        req->pre_state = state;
        return 0;
    }

    static int OnBody(http_parser *p, const char *buf, size_t len) {
        auto req = static_cast<HttpRequest *>(p->data);
        req->body.append(buf, len);
        return 0;
    }

    static int OnHeaderComplete(http_parser *p, const char *buf, size_t len) {
        auto req = static_cast<HttpRequest *>(p->data);
        http_parser_parse_url(req->url.data(), req->url.size(), 1, &req->u);
        req->parse_query(req->url_query());
        auto & conn = req->field_value["Connection"];
        if (conn != "Upgrade") {
            return 0;
        }
        auto & up = req->field_value["Upgrade"];
        if (up != "websocket") {
            return 0;
        }
        auto & key = req->field_value["Sec-WebSocket-Key"];
        if (key.empty()) {
            return 0;
        }
        req->websocket_upgrade_ = true;
        req->websocket_key_ = key;
        return 0;
    }

    static int EmptyCB(http_parser *p) {
        return 0;
    }

    static int EmptyDataCB(http_parser *p, const char *buf, size_t len) {
        return 0;
    }
private:
    Buffer buffer_;
    std::string field;
    std::string value;
    std::string url;
    std::string method_;

    unsigned char pre_state{0};
    bool is_completed{false};
    bool send_continue_{false};
    http_parser_settings settings;
    http_parser_url  u;
    bool is_keep_alive_{false};

    bool websocket_upgrade_{false};
    std::string websocket_key_;
};

}
}

