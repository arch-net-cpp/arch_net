#include "coin_client.h"

namespace arch_net { namespace coin {

// Request implementation
inline bool Request::has_header(const std::string &key) const {
    return ::arch_net::coin::has_header(headers, key);
}

inline std::string Request::get_header_value(const std::string &key) const {
    return ::arch_net::coin::get_header_value(headers, key, "");
}

inline void Request::set_header(const std::string &key,
                                const std::string &val) {
    if (!::arch_net::coin::has_crlf(key) && !::arch_net::coin::has_crlf(val)) {
        headers.emplace(key, val);
    }
}

inline bool Request::has_param(const std::string &key) const {
    return params.find(key) != params.end();
}

inline std::string Request::get_param_value(const std::string &key,
                                            size_t id) const {
    auto rng = params.equal_range(key);
    auto it = rng.first;
    std::advance(it, static_cast<ssize_t>(id));
    if (it != rng.second) { return it->second; }
    return std::string();
}

inline bool Request::is_multipart_form_data() const {
    const auto &content_type = get_header_value("Content-Type");
    return !content_type.rfind("multipart/form-data", 0);
}

inline bool Request::has_file(const std::string &key) const {
    return files.find(key) != files.end();
}

inline MultipartFormData Request::get_file_value(const std::string &key) const {
    auto it = files.find(key);
    if (it != files.end()) { return it->second; }
    return MultipartFormData();
}

// Response implementation
Response::Response() {
    settings.on_message_begin = &Response::OnMessageBegin;
    settings.on_message_complete = &Response::OnMessageEnd;
    settings.on_header_field = &Response::OnField;
    settings.on_header_value = &Response::OnValue;
    settings.on_body = &Response::OnBody;
    settings.on_reason = &Response::OnReason;
    settings.on_headers_complete = &Response::OnHeaderComplete;
    settings.on_reason = &Response::EmptyDataCB;
    settings.on_chunk_header = &Response::EmptyCB;
    settings.on_chunk_complete = &Response::EmptyCB;
    http_parser_init(&parser, HTTP_RESPONSE);
}

inline bool Response::has_header(const std::string &key) const {
    return headers.find(key) != headers.end();
}

inline std::string Response::get_header_value(const std::string &key,
                                              size_t id) const {
    return ::arch_net::coin::get_header_value(headers, key, "");
}

inline void Response::set_header(const std::string &key,
                                 const std::string &val) {
    if (!::arch_net::coin::has_crlf(key) && !::arch_net::coin::has_crlf(val)) {
        headers.emplace(key, val);
    }
}

inline void Response::set_redirect(const std::string &url, int stat) {
    if (!::arch_net::coin::has_crlf(url)) {
        set_header("Location", url);
        if (300 <= stat && stat < 400) {
            this->status_code = stat;
        } else {
            this->status_code = 302;
        }
    }
}

inline void Response::set_content(const char *s, size_t n,
                                  const std::string &content_type) {
    body.assign(s, n);

    auto rng = headers.equal_range("Content-Type");
    headers.erase(rng.first, rng.second);
    set_header("Content-Type", content_type);
}

inline void Response::set_content(const std::string &s,
                                  const std::string &content_type) {
    set_content(s.data(), s.size(), content_type);
}

int Response::parse(Buffer *buffer) {
    parser.data = const_cast<Response *>(this);
    size_t parsed = http_parser_execute(&parser, &settings, buffer->data(), buffer->size());
    auto err = HTTP_PARSER_ERRNO(&parser);
    if (err != HPE_OK && err != HPE_PAUSED) {
        LOG(ERROR) << "http request header parsed failed, err=" << http_errno_name(err) << "," << http_errno_description(err);
        return err;
    }
    buffer->Retrieve(parsed);
    return 0;
}

// Result implementation
inline bool Result::has_request_header(const std::string &key) const {
    return request_headers_.find(key) != request_headers_.end();
}

inline std::string Result::get_request_header_value(const std::string &key,
                                                    size_t id) const {
    return coin::get_header_value(request_headers_, key, "");
}

inline std::string Result::get_response_header_value(const std::string &key,
                                                     size_t id) const {
    if (res_) {
        return coin::get_header_value(res_->headers, key, "");
    }
    return "";
}


// HTTP client implementation

CoinClient::CoinClient(const std::string &host, int port)
    : CoinClient(host, port, std::string(), std::string(), std::string(), false) {}

CoinClient::CoinClient(const std::string &host, int port,
                       const std::string &ca_cert_path,
                       const std::string &client_cert_path,
                       const std::string &client_key_path, bool must_ssl)
    : host_(host), port_(port), host_and_port_(adjust_host_string(host) + ":" + std::to_string(port)),
      client_cert_path_(client_cert_path), client_key_path_(client_key_path),
      ca_cert_path_(ca_cert_path), application_client_(new ApplicationClient) {

    ClientOption option;
    if (must_ssl) {
        option.tls_ctx = ssl::new_client_tls_context(ca_cert_path, client_cert_path, client_key_path);
        if (option.tls_ctx == nullptr) {
            throw std::invalid_argument("invalid tls config");
        }
    }
    option.connection_type = ConnectionType::Pooled;
    option.connection_timeout = 5000;
    option.read_write_timeout = 5000;
    // init application client
    application_client_->init(host, port, option);
}

CoinClient::CoinClient(const std::string &host)
    : CoinClient(host, std::string(), std::string(), std::string()) {}

CoinClient::CoinClient(const std::string &scheme_host_port,
                       const std::string &ca_cert_path,
                       const std::string &client_cert_path,
                       const std::string &client_key_path)
    : application_client_(new ApplicationClient) {

    const static std::regex re(
            R"((?:([a-z]+):\/\/)?(?:\[([\d:]+)\]|([^:/?#]+))(?::(\d+))?)");

    std::smatch m;

    ClientOption option;
    option.connection_type = ConnectionType::Pooled;
    option.connection_timeout = 5000;
    option.read_write_timeout = 5000;

    if (std::regex_match(scheme_host_port, m, re)) {
        auto scheme = m[1].str();
        if (!scheme.empty() && (scheme != "http" && scheme != "https")) {
            std::string msg = "'" + scheme + "' scheme is not supported.";
            throw std::invalid_argument(msg);
        }

        auto is_ssl = scheme == "https";
        auto host = m[2].str();
        if (host.empty()) { host = m[3].str(); }
        auto port_str = m[4].str();
        auto port = !port_str.empty() ? std::stoi(port_str) : (is_ssl ? 443 : 80);

        if (is_ssl) {
            option.tls_ctx = ssl::new_client_tls_context(ca_cert_path, client_cert_path, client_key_path);
            if (option.tls_ctx == nullptr) {
                throw std::invalid_argument("invalid tls config");
            }
        }
        // application client init
        application_client_->init(string_printf("%s:%d", host.c_str(), port), option);
    } else {
        application_client_->init(scheme_host_port, 80, option);
    }
}

inline CoinClient::~CoinClient() {}

std::unique_ptr<CoinClient::Session> CoinClient::new_session() {
    auto f = application_client_->get_connection();
    if (f == nullptr) {
        return nullptr;
    }
    auto session = std::make_unique<CoinClient::Session>(f, this);
    return session;
}

WorkerPool *CoinClient::get_workers() {
    workers_mutex_.lock();
    defer(workers_mutex_.unlock());
    if (!workers_) {
        workers_ = std::make_unique<IOWorkerPool>();
    }
    return workers_.get();
}

Future<Error, bool> CoinClient::send(Session* session, Request &req, Response &res, Error &error) {
    for (const auto &header : default_headers_) {
        if (req.headers.find(header.first) == req.headers.end()) {
            req.headers.insert(header);
        }
    }
    auto close_connection = !keep_alive_;
    res.content_receiver = std::move(req.content_receiver);

    Promise<Error, bool> promise;
    session->connection()->custom_call([&, session = session, promise = promise, &req = req, &res = res, &error = error]() {
        auto ret = handle_request(session, req, res, close_connection, error);
        if (!ret) {
            if (error == Error::Success) { error = Error::Unknown; }
        }
        promise.setValue(error, ret);
    });

    return promise.getFuture();
}

inline Result CoinClient::send_content(Session* session,Request &&req) {
    auto res = std::make_unique<Response>();
    auto error = Error::Success;

    auto ret = send(session, req, *res, error);
    bool send_result;
    auto err = ret.get(send_result);
    return Result{send_result ? std::move(res) : nullptr, err, std::move(req.headers)};
}

inline bool CoinClient::handle_request(Session* session, Request &req,
                                       Response &res, bool close_connection,
                                       Error &error) {
    if (req.path.empty()) {
        error = Error::Connection;
        return false;
    }

    auto req_save = req;

    bool ret = process_request(session, req, res, close_connection, error);

    if (!ret) { return false; }

    if (300 < res.status_code && res.status_code < 400 && follow_location_) {
        req = req_save;
        ret = redirect(req, res, error);
    }

    return ret;
}

inline bool CoinClient::redirect(Request &req, Response &res, Error &error) {

    auto location = decode_url(res.get_header_value("location"), true);
    if (location.empty()) { return false; }

    const static std::regex re(
            R"((?:(https?):)?(?://(?:\[([\d:]+)\]|([^:/?#]+))(?::(\d+))?)?([^?#]*(?:\?[^#]*)?)(?:#.*)?)");

    std::smatch m;
    if (!std::regex_match(location, m, re)) { return false; }

    auto scheme = is_ssl() ? "https" : "http";

    auto next_scheme = m[1].str();
    auto next_host = m[2].str();
    if (next_host.empty()) { next_host = m[3].str(); }
    auto port_str = m[4].str();
    auto next_path = m[5].str();

    auto next_port = port_;
    if (!port_str.empty()) {
        next_port = std::stoi(port_str);
    } else if (!next_scheme.empty()) {
        next_port = next_scheme == "https" ? 443 : 80;
    }
    (void) next_port;
    if (next_scheme.empty()) { next_scheme = scheme; }
    if (next_host.empty()) { next_host = host_; }
    if (next_path.empty()) { next_path = "/"; }
//    if (next_scheme == scheme && next_host == host_ && next_port == port_) {
//        return redirect(*this, req, res, next_path, location, error);
//    }  else {
//            CoinClient cli(next_host.c_str(), next_port);
//            cli.copy_settings(*this);
//            return redirect(cli, req, res, next_path, location, error);
//        }
//    }
    return true;
}

inline bool CoinClient::write_request(Session* session, Request &req,
                                      bool close_connection, Error &error) {
    // Prepare additional headers
    if (close_connection) {
        if (!req.has_header("Connection")) {
            req.headers.emplace("Connection", "close");
        }
    }

    if (!req.has_header("Host")) {
        if (is_ssl()) {
            if (port_ == 443) {
                req.headers.emplace("Host", host_);
            } else {
                req.headers.emplace("Host", host_and_port_);
            }
        } else {
            if (port_ == 80) {
                req.headers.emplace("Host", host_);
            } else {
                req.headers.emplace("Host", host_and_port_);
            }
        }
    }

    if (!req.has_header("Accept")) { req.headers.emplace("Accept", "*/*"); }

    if (!req.has_header("User-Agent")) {
        auto agent = std::string("coin_client/") + "1.0";
        req.headers.emplace("User-Agent", agent);
    }

    if (req.body.empty()) {
        if (req.method == "POST" || req.method == "PUT" ||
            req.method == "PATCH") {
            req.headers.emplace("Content-Length", "0");
        }
    } else {
        if (!req.has_header("Content-Type")) {
            req.headers.emplace("Content-Type", "text/plain");
        }

        if (!req.has_header("Content-Length")) {
            auto length = std::to_string(req.body.size());
            req.headers.emplace("Content-Length", length);
        }
    }

    if (!basic_auth_password_.empty() || !basic_auth_username_.empty()) {
        if (!req.has_header("Authorization")) {
            req.headers.insert(make_basic_authentication_header(
                    basic_auth_username_, basic_auth_password_, false));
        }
    }

    if (!bearer_token_auth_token_.empty()) {
        if (!req.has_header("Authorization")) {
            req.headers.insert(make_bearer_token_authentication_header(
                    bearer_token_auth_token_, false));
        }
    }

    // Request line and headers
    {
        Buffer buffer;
        const auto &path = url_encode_ ? encode_url(req.path) : req.path;
        buffer.write_format("%s %s HTTP/1.1\r\n", req.method.c_str(), path.c_str());
        for (const auto &x : req.headers) {
            buffer.write_format("%s: %s\r\n", x.first.c_str(), x.second.c_str());
        }
        buffer.Append("\r\n");

        if (session->connection()->get_stream()->send(&buffer) <= 0) {
            error = Error::Write;
            return false;
        }
    }

    // Body
    if (!req.body.empty() && session->connection()->get_stream()->send(req.body.data(), req.body.size() <= 0) ) {
        error = Error::Write;
        return false;
    }

    return true;
}

inline std::unique_ptr<Response> CoinClient::send_with_content_provider(
        Session* session,Request &req, const std::string& body, size_t content_length,
        const std::string &content_type, Error &error) {

    if (!content_type.empty()) {
        req.headers.emplace("Content-Type", content_type);
    }

    if (compress_) { req.headers.emplace("Content-Encoding", "gzip"); }

    if (compress_) {
        gzip_compressor compressor;
        if (!compressor.compress(body.c_str(), content_length, true,
            [&](const char *data, size_t data_len) {
                req.body.append(data, data_len);
                return true;
            })) {
            error = Error::Compression;
            return nullptr;
        }
    } else {
        req.body.assign(body, content_length);
    }

    auto res = std::make_unique<Response>();

    auto ret = send(session, req, *res, error);
    bool send_result;
    auto err = ret.get(send_result);

    return send_result ? std::move(res) : nullptr;
}

inline Result CoinClient::send_with_content_provider(
        Session* session,const std::string &method, const std::string &path,
        const Headers &headers, const std::string& body, size_t content_length,
        const std::string &content_type) {

    Request req;
    req.method = method;
    req.headers = headers;
    req.path = path;

    auto error = Error::Success;

    auto res = send_with_content_provider(
            session, req, body, content_length, content_type, error);

    return Result{std::move(res), error, std::move(req.headers)};
}

inline std::string
CoinClient::adjust_host_string(const std::string &host) const {
    if (host.find(':') != std::string::npos) { return "[" + host + "]"; }
    return host;
}

bool CoinClient::recv_and_parse(Session* session, Buffer *buffer,  Response &res) {
    while (true) {
        auto ret = session->connection()->get_stream()->recv(buffer);
        if (ret <= 0) {
            // error or close
            return false;
        }
        LOG(INFO) << "recv http response: " << buffer->ToString();
        if (res.parse(buffer) != 0) {
            return false;
        }
        if (res.is_completed) {
            return true;
        }
    }
}

inline bool CoinClient::process_request(Session* session, Request &req,
                                        Response &res, bool close_connection,
                                        Error &error) {
    // Send request
    if (!write_request(session, req, close_connection, error)) { return false; }

    // Receive response and parse
    Buffer buffer(2048, 0);
    if (!recv_and_parse(session, &buffer, res)) {
        error = Error::Read;
        return false;
    }

    if (res.get_header_value("Connection") == "close" ||
        (res.version == "HTTP/1.0" && res.reason != "Connection established")) {
    }
    // Log

    return true;
}


//inline Result CoinClient::Session::Get(const std::string &path) {
//    return Get(path, Headers());
//}

//inline Result CoinClient::Session::Get(const std::string &path, const Headers &headers) {
//    Request req;
//    req.method = "GET";
//    req.path = path;
//    req.headers = headers;
//
//    return client_->send_(this, std::move(req));
//}
//
//inline Result CoinClient::Session::Get(const std::string &path, ContentReceiver content_receiver) {
//    return Get(path, Headers(), std::move(content_receiver));
//}

inline Result CoinClient::Session::Get(const std::string &path, const Headers &headers, ContentReceiver content_receiver) {
    Request req;
    req.method = "GET";
    req.path = path;
    req.headers = headers;
    req.content_receiver = std::move(content_receiver);
    return client_->send_content(this, std::move(req));
}

//Result CoinClient::Session::Get(const std::string &path, const Params &params, const Headers &headers) {
//
//    if (!params.empty()) {
//        std::string path_with_query = append_query_params(path, params);
//        return Get(path_with_query, headers);
//    }
//
//    return Get(path, headers);
//}

Result CoinClient::Session::Get(const std::string &path, const Params &params, const Headers &headers,
                                ContentReceiver content_receiver) {
    if (!params.empty()) {
        std::string path_with_query = append_query_params(path, params);
        return Get(path_with_query, headers, std::move(content_receiver));
    }

    return Get(path, headers, std::move(content_receiver));
}

Future<Error, ResultPtr>
CoinClient::Session::Future_Get(const std::string &path, const Params &params, const Headers &headers,
                                ContentReceiver content_receiver) {
    Promise<Error, ResultPtr> promise;
    this->connection()->custom_call([&, promise = promise]() {
        auto result = Get(path, params, headers, std::move(content_receiver));
        auto err = result.error();
        promise.setValue(err, std::make_shared<Result>(std::move(result)));
    });
    return promise.getFuture();
}


inline Result CoinClient::Session::Head(const std::string &path) {
    return Head(path, Headers());
}

inline Result CoinClient::Session::Head(const std::string &path, const Headers &headers) {
    Request req;
    req.method = "HEAD";
    req.headers = headers;
    req.path = path;

    return client_->send_content(this, std::move(req));
}

// normal body

Result CoinClient::Session::Post(const std::string &path) {
    return Post(path, Headers(), Params());
}

Result CoinClient::Session::Post(const std::string &path, const Headers &headers) {
    return Post(path, headers, Params());
}

inline Result CoinClient::Session::Post(const std::string &path, const Params &params) {
    return Post(path, Headers(), params);
}

inline Result CoinClient::Session::Post(const std::string &path, const Headers &headers, const Params &params) {
    auto query = params_to_query_str(params);
    return Post(path, headers, query, "application/x-www-form-urlencoded");
}

inline Result CoinClient::Session::Post(const std::string &path, const std::string &body, const std::string &content_type) {
    return Post(path, Headers(), body, content_type);
}


inline Result CoinClient::Session::Post(const std::string &path, const MultipartFormDataItems &items) {
    return Post(path, Headers(), items);
}

Future<Error, ResultPtr>
CoinClient::Session::Future_Post(const std::string &path, const Headers &headers = {}, const std::string &body = "",
                                 const std::string &content_type = "") {
    Promise<Error, ResultPtr> promise;
    this->connection()->custom_call([&, promise = promise]() {
        auto result = Post(path, headers, body, content_type);
        auto err = result.error();
        promise.setValue(err, std::make_shared<Result>(std::move(result)));
    });
    return promise.getFuture();
}

// multipart body
inline Result CoinClient::Session::Post(const std::string &path, const Headers &headers,
                                        const MultipartFormDataItems &items) {
    const auto &boundary = make_multipart_data_boundary();
    const auto &content_type =
            serialize_multipart_formdata_get_content_type(boundary);
    const auto &body = serialize_multipart_formdata(items, boundary);
    return Post(path, headers, body, content_type.c_str());
}

inline Result CoinClient::Session::Post(const std::string &path, const Headers &headers,
                                        const MultipartFormDataItems &items,
                                        const std::string &boundary) {
    if (!is_multipart_boundary_chars_valid(boundary)) {
        return Result{nullptr, Error::UnsupportedMultipartBoundaryChars};
    }

    const auto &content_type =
            serialize_multipart_formdata_get_content_type(boundary);
    const auto &body = serialize_multipart_formdata(items, boundary);
    return Post(path, headers, body, content_type.c_str());
}

inline Result CoinClient::Session::Post(const std::string &path, const Headers &headers, const std::string &body,
                                        const std::string &content_type) {

    return client_->send_with_content_provider(this, "POST", path, headers, body.data(), body.size(), content_type);
}


inline Result CoinClient::Session::Put(const std::string &path, const char *body,
                                       size_t content_length,
                                       const std::string &content_type) {
    return Put(path, Headers(), body, content_length, content_type);
}

inline Result CoinClient::Session::Put(const std::string &path, const Headers &headers,
                              const char *body, size_t content_length,
                              const std::string &content_type) {
    return client_->send_with_content_provider(this, "PUT", path, headers, body, content_length,content_type);
}

inline Result CoinClient::Session::Put(const std::string &path, const std::string &body,
                              const std::string &content_type) {
    return Put(path, Headers(), body, content_type);
}

inline Result CoinClient::Session::Put(const std::string &path, const Headers &headers,
                              const std::string &body,
                              const std::string &content_type) {
    return client_->send_with_content_provider(this, "PUT", path, headers, body.data(),
                                      body.size(), content_type);
}

inline Result CoinClient::Session::Put(const std::string &path, const Headers &headers,
                              const Params &params) {
    auto query = params_to_query_str(params);
    return Put(path, headers, query, "application/x-www-form-urlencoded");
}

inline Result CoinClient::Session::Put(const std::string &path, const Headers &headers,
                              const MultipartFormDataItems &items) {
    const auto &boundary = make_multipart_data_boundary();
    const auto &content_type =
            serialize_multipart_formdata_get_content_type(boundary);
    const auto &body = serialize_multipart_formdata(items, boundary);
    return Put(path, headers, body, content_type);
}

inline Result CoinClient::Session::Put(const std::string &path, const Headers &headers,
                              const MultipartFormDataItems &items,
                              const std::string &boundary) {
    if (!is_multipart_boundary_chars_valid(boundary)) {
        return Result{nullptr, Error::UnsupportedMultipartBoundaryChars};
    }

    const auto &content_type =
            serialize_multipart_formdata_get_content_type(boundary);
    const auto &body = serialize_multipart_formdata(items, boundary);
    return Put(path, headers, body, content_type);
}


inline Result CoinClient::Session::Patch(const std::string &path) {
    return Patch(path, std::string(), std::string());
}

inline Result CoinClient::Session::Patch(const std::string &path, const char *body,
                                size_t content_length,
                                const std::string &content_type) {
    return Patch(path, Headers(), body, content_length, content_type);
}

inline Result CoinClient::Session::Patch(const std::string &path, const Headers &headers,
                                const char *body, size_t content_length,
                                const std::string &content_type) {
    return client_->send_with_content_provider(this, "PATCH", path, headers, body,
                                      content_length, content_type);
}

inline Result CoinClient::Session::Patch(const std::string &path,
                                const std::string &body,
                                const std::string &content_type) {
    return Patch(path, Headers(), body, content_type);
}

inline Result CoinClient::Session::Patch(const std::string &path, const Headers &headers,
                                const std::string &body,
                                const std::string &content_type) {
    return client_->send_with_content_provider(this, "PATCH", path, headers, body.data(),
                                      body.size(), content_type);
}


inline Result CoinClient::Session::Delete(const std::string &path) {
    return Delete(path, Headers(), std::string(), std::string());
}

inline Result CoinClient::Session::Delete(const std::string &path,
                                 const Headers &headers) {
    return Delete(path, headers, std::string(), std::string());
}

inline Result CoinClient::Session::Delete(const std::string &path, const char *body,
                                 size_t content_length,
                                 const std::string &content_type) {
    return Delete(path, Headers(), body, content_length, content_type);
}

inline Result CoinClient::Session::Delete(const std::string &path,
                                 const Headers &headers, const char *body,
                                 size_t content_length,
                                 const std::string &content_type) {
    Request req;
    req.method = "DELETE";
    req.headers = headers;
    req.path = path;

    if (!content_type.empty()) {
        req.headers.emplace("Content-Type", content_type);
    }
    req.body.assign(body, content_length);

    return client_->send_content(this, std::move(req));
}

inline Result CoinClient::Session::Delete(const std::string &path,
                                 const std::string &body,
                                 const std::string &content_type) {
    return Delete(path, Headers(), body.data(), body.size(), content_type);
}

inline Result CoinClient::Session::Delete(const std::string &path,
                                 const Headers &headers,
                                 const std::string &body,
                                 const std::string &content_type) {
    return Delete(path, headers, body.data(), body.size(), content_type);
}

inline Result CoinClient::Session::Options(const std::string &path) {
    return Options(path, Headers());
}

inline Result CoinClient::Session::Options(const std::string &path,
                                  const Headers &headers) {
    Request req;
    req.method = "OPTIONS";
    req.headers = headers;
    req.path = path;

    return client_->send_content(this, std::move(req));
}

Result CoinClient::Session::WebSocket(const std::string &path, const Params &params,
                                      const Headers &headers, WebSocketCallback callback) {

    if (!check_ws_callback(callback)) {
        return Result{nullptr, Error::WebsocketError};
    }

    auto new_headers = headers;
    new_headers.emplace("Connection", "Upgrade");
    new_headers.emplace("Upgrade", "websocket");
    new_headers.emplace("Sec-WebSocket-Version", "13");
    auto websocket_key = Crypto::Base64::encode(gen_random_str());
    new_headers.emplace("Sec-WebSocket-Key", websocket_key);

    Promise<int, int> promise;
    auto result = Get(path, params, new_headers, nullptr);
    if (result.error() == Error::Success && result) {
        // check server accept key
        static auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        auto sha1 = Crypto::sha1(websocket_key + ws_magic_string);
        auto accept_key = Crypto::Base64::encode(sha1);

        if (result->get_header_value("Sec-WebSocket-Accept") == accept_key) {
            this->connection()->custom_call([&, callback = std::move(callback)]() mutable {
                auto connection = std::make_shared<WebSocketConnection>
                        (connection_->get_stream(), callback, client_->get_workers(), false);
                connection->process();
                promise.setValue(1, 1);
            });
        } else {
            promise.setValue(0, 0);
        }
    } else {
        promise.setValue(0, 0);
    }

    auto f = promise.getFuture();
    int val;
    f.get(val);
    return result;
}

bool CoinClient::Session::check_ws_callback(WebSocketCallback &callback) {
    if (callback.OnAuth == nullptr) return false;
    if (callback.OnOpen == nullptr) return false;
    if (callback.OnClose == nullptr) return false;
    if (callback.OnError == nullptr) return false;
    if (callback.OnMessage == nullptr) return false;
    if (callback.OnPing == nullptr) return false;
    if (callback.OnPong == nullptr) return false;
    return true;
}

}}