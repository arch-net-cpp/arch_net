#include "response.h"

namespace arch_net { namespace coin{

void HttpResponse::json(int status_code, const std::string &data) {
    buffer_.Reset();
    http_status_code_ = status_code;
    set_header("Content-Type", "application/json; charset=utf-8");
    make_response(data);
}

void HttpResponse::text(int status_code, const std::string &data) {
    buffer_.Reset();
    http_status_code_ = status_code;
    set_header("Content-Type", "text/plain; charset=UTF-8");
    make_response(data);
}

void HttpResponse::html(int status_code, const std::string &data) {
    buffer_.Reset();
    http_status_code_ = status_code;
    set_header("Content-Type", "text/html; charset=UTF-8");
    make_response(data);
}

void HttpResponse::add_content_len(const int64_t size) {
    char len[42];
    snprintf(len, sizeof(len), "Content-Length:%ld\r\n", size) ;
    buffer_.Append(len, strlen(len));
}

void HttpResponse::add_date() {
    char date[50];
    struct tm cur;
    struct tm *cur_p;
    time_t t = time(NULL);
    gmtime_r(&t, &cur);
    cur_p = &cur;
    if (strftime(date, sizeof(date),
                 "Date:%a, %d %b %Y %H:%M:%S GMT\r\n", cur_p) != 0) {
        buffer_.Append(date, strlen(date));
    }
}

void HttpResponse::make_response(const std::string& body) {
    //HTTP/%d.%d code reason\r\n
    auto response_code_iter = http_status_code.find(http_status_code_);
    if (response_code_iter == http_status_code.end()) {
        response_code_iter = http_status_code.find(808);
    }

    char status[16];
    snprintf(status, sizeof status, "HTTP/%d.%d %d ", 1, 1, response_code_iter->first);
    buffer_.Append(status);
    buffer_.Append(response_code_iter->second);
    buffer_.Append("\r\n");
    if (http_status_code_ == 400) { //Bad request
        buffer_.Append("\r\n");
        return;
    }
    add_date();
    add_content_len(body.size());
    for (auto & it : headers_) {
        buffer_.Append(it.first);
        buffer_.Append(":");
        buffer_.Append(it.second);
        buffer_.Append("\r\n");
    }
    buffer_.Append("\r\n");
    // append body
    buffer_.Append(body);
}

int HttpResponse::send(ISocketStream* stream) {
    auto rc = stream->send(buffer_.data(), buffer_.length());
    buffer_.Reset();
    return rc;
}

}}