#pragma once
#include "../buffer.h"
#include "request.h"

namespace arch_net { namespace coin {

static std::unordered_map<int, std::string> http_status_code = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Time-out"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Large"},
    {415, "Unsupported Media Type"},
    {416, "Requested range not satisfiable"},
    {417, "Expectation Failed"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Time-out"},
    {505, "HTTP Version not supported"},
    {808, "UnKnown"}  //private
};

class HttpResponse {

public:

    HttpResponse() {};

    virtual ~HttpResponse() = default;

    int send(ISocketStream* stream);

    void set_header(const std::string& key, const std::string& value) { headers_[key] = value; }

    int status_code() const { return http_status_code_; }

    void html(int status_code, const std::string &data);

    void text(int status_code, const std::string &data);

    void json(int status_code, const std::string &data);

    friend std::ostream& operator<<(std::ostream& os, const HttpResponse& res) {
        os << res.buffer_.ToString();
        return os;
    }

    bool keep_alive() const {
        return keep_alive_;
    }

    int keep_alive(bool ka) {
        keep_alive_ = ka;
        if (!keep_alive_) {
            set_header("Connection", "close");
        }
        return 0;
    }

    void reset() {
        buffer_.Reset();
        headers_.clear();
        http_status_code_ = -1;
        keep_alive_ = false;
    }

private:
    void make_response(const std::string& body);
    void add_date();
    void add_content_len(const int64_t size);

private:
    int http_status_code_{-1};
    std::unordered_map<std::string, std::string> headers_;
    Buffer buffer_;
    bool keep_alive_;
};


}}