
#include "request.h"

namespace arch_net { namespace coin {
HttpRequest::HttpRequest() {
    settings.on_message_begin = &HttpRequest::OnMessageBegin;
    settings.on_message_complete = &HttpRequest::OnMessageEnd;
    settings.on_header_field = &HttpRequest::OnField;
    settings.on_header_value = &HttpRequest::OnValue;
    settings.on_body = &HttpRequest::OnBody;
    settings.on_url = &HttpRequest::OnUrl;
    settings.on_headers_complete = &HttpRequest::OnHeaderComplete;
    settings.on_reason = &HttpRequest::EmptyDataCB;
    settings.on_chunk_header = &HttpRequest::EmptyCB;
    settings.on_chunk_complete = &HttpRequest::EmptyCB;
    http_parser_init(&parser, HTTP_REQUEST);
}

int HttpRequest::recv_and_parse(ISocketStream* stream) {
    while (true) {
        auto ret = buffer_.ReadFromSocketStream(stream);
        if (ret <= 0) {
            // error or close
            return ERR;
        }
        LOG(INFO) << "recv http request: " << buffer_.ToString();
        if (parse(buffer_) != 0) {
            return ERR;
        }
        if (completed()) {
            return OK;
        }
    }
}

int HttpRequest::parse(Buffer& buffer) {
    parser.data = const_cast<HttpRequest *>(this);
    size_t parsed = http_parser_execute(&parser, &settings, buffer.data(), buffer.size());
    auto err = HTTP_PARSER_ERRNO(&parser);
    if (err != HPE_OK && err != HPE_PAUSED) {
        LOG(ERROR) << "http request header parsed failed, err=" << http_errno_name(err) << "," << http_errno_description(err);
        return err;
    }
    buffer.Retrieve(parsed);
    return 0;
}

std::string HttpRequest::url_path() const {
    if ((u.field_set & (1 << UF_PATH)) != 0) {
        return std::string(url, u.field_data[3].off,  u.field_data[3].len);
    }
    return "";
}

std::string HttpRequest::url_fragment() const {
    if ((u.field_set & (1 << UF_FRAGMENT)) != 0) {
        return std::string(url, u.field_data[5].off,  u.field_data[5].len);
    }
    return "";
}

std::string HttpRequest::url_query() const {
    if ((u.field_set & (1 << UF_QUERY)) != 0) {
        return std::string(url, u.field_data[4].off,  u.field_data[4].len);
    }
    return "";
}

std::string HttpRequest::url_userinfo() const{
    if ((u.field_set & (1 << UF_USERINFO)) != 0) {
        return std::string(url, u.field_data[6].off,  u.field_data[6].len);
    }
    return "";
}

void HttpRequest::swap(HttpRequest &hr) {
    body.swap(hr.body);
    field_value.swap(hr.field_value);
    parser = hr.parser;
    remote_ip.swap(hr.remote_ip);
    field.swap(hr.field);
    value.swap(hr.value);
    url.swap(hr.url);
    pre_state = hr.pre_state;
    is_completed = hr.is_completed;
    settings = hr.settings;
    send_continue_ = hr.send_continue_;
    u = hr.u;
}

std::string HttpRequest::get_header(const std::string& key) const{
    if (key.empty()) return "";
    auto it = field_value.find(key);
    if (it == field_value.end()) return "";
    return it->second;
}

std::string HttpRequest::get_query(const std::string& key) const{
    if (key.empty()) return "";
    auto it = query.find(key);
    if (it == query.end()) return "";
    return it->second;
}

std::string HttpRequest::get_params(const std::string& key) const{
    if (key.empty()) return "";
    auto it = params.find(key);
    if (it == params.end()) return "";
    return it->second;
}

void HttpRequest::parse_query(const std::string& str) {
    std::vector<slice> kv_pairs;
    split(kv_pairs, str, '&');
    for (auto & pair : kv_pairs) {
        std::vector<slice> kv;
        split(kv, pair, '=');
        if (kv.size() != 2)
            continue;
        query[kv[0].ToString()] = kv[1].ToString();
    }
}

void HttpRequest::reset() {
    body.clear();
    field_value.clear();

    remote_ip.clear();
    params.clear();
    query.clear();
    buffer_.Reset();
    field.clear();
    value.clear();
    url.clear();
    method_.clear();

    is_completed = false;
    send_continue_ = false;
    is_keep_alive_ = false;

    http_parser_init(&parser, HTTP_REQUEST);
}

}}