#pragma once
#include "../common.h"
#include "../socket_stream.h"
#include "../ssl_socket_stream.h"
#include "../application_client.h"
#include "http_utils.h"
#include "http_parser.h"
#include "../buffer.h"
#include <map>
#include "websocket.h"

namespace arch_net { namespace coin {

using Headers = std::multimap<std::string, std::string, cmp>;
using Params = std::multimap<std::string, std::string>;
using Match = std::smatch;
using ContentReceiver = std::function<bool(const char *data, size_t data_length)>;
using MultipartFormDataItems = std::vector<MultipartFormData>;
using MultipartFormDataMap = std::multimap<std::string, MultipartFormData>;

enum class Error {
    Success = 0,
    Unknown,
    Connection,
    BindIPAddress,
    Read,
    Write,
    ExceedRedirectCount,
    Canceled,
    SSLConnection,
    SSLLoadingCerts,
    SSLServerVerification,
    UnsupportedMultipartBoundaryChars,
    Compression,
    ConnectionTimeout,
    WebsocketError,
};

struct Request {
    std::string method;
    std::string path;
    Headers headers;
    std::string body;

    std::string remote_addr;
    int remote_port = -1;
    std::string local_addr;
    int local_port = -1;

    ContentReceiver content_receiver;

    // for server
    std::string version;
    std::string target;
    Params params;
    MultipartFormDataMap files;
    Match matches;

    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key) const;
    void set_header(const std::string &key, const std::string &val);

    bool has_param(const std::string &key) const;
    std::string get_param_value(const std::string &key, size_t id = 0) const;

    bool is_multipart_form_data() const;

    bool has_file(const std::string &key) const;
    MultipartFormData get_file_value(const std::string &key) const;

    // private members...
    size_t content_length_ = 0;
    bool is_chunked_content_provider_ = false;
    size_t authorization_count_ = 0;
};

struct Response {
    std::string version;
    int status_code = -1;
    std::string reason;
    Headers headers;
    std::string body;
    std::string location; // Redirect location
    bool is_completed{false};

    ContentReceiver content_receiver;

    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key, size_t id = 0) const;
    template <typename T>
    T get_header_value(const std::string &key, size_t id = 0) const;
    void set_header(const std::string &key, const std::string &val);

    void set_redirect(const std::string &url, int status = 302);
    void set_content(const char *s, size_t n, const std::string &content_type);
    void set_content(const std::string &s, const std::string &content_type);

    Response();
    Response(const Response &) = default;
    Response &operator=(const Response &) = default;
    Response(Response &&) = default;
    Response &operator=(Response &&) = default;
    ~Response() {}

    int parse(Buffer* buffer);

    static int OnMessageBegin(http_parser *p) {return 0;}

    static int OnMessageEnd(http_parser *p) {
        auto resp = static_cast<Response *>(p->data);
        resp->is_completed = true;
        http_parser_pause(p, 1);
        resp->status_code = p->status_code;
        return 0;
    }

    static int OnUrl(http_parser *p, const char *buf, size_t len) {return 0;}

    static int OnReason(http_parser *p, const char *buf, size_t len) {
        auto resp = static_cast<Response *>(p->data);
        resp->reason.append(buf, len);
        return 0;
    }

    static int OnField(http_parser *p, const char *buf, size_t len) {
        auto resp = static_cast<Response *>(p->data);
        if (resp->pre_state == 51) {
            resp->field.append(buf, len);
        } else {
            resp->field.assign(buf, len);
        }
        resp->pre_state = p->state;
        return 0;
    }

    static int OnValue(http_parser *p, const char *buf, size_t len) {
        auto resp = static_cast<Response *>(p->data);
        if (resp->pre_state == 53/**/) {
            resp->value.append(buf, len);
        } else {
            resp->value.assign(buf, len);
        }

        unsigned char state = p->state;
        if (state == 55/*head value*/) {
            resp->headers.emplace(std::move(resp->field), std::move(resp->value));
            resp->field.clear();
        }
        resp->pre_state = state;
        return 0;
    }

    static int OnBody(http_parser *p, const char *buf, size_t len) {
        auto resp = static_cast<Response *>(p->data);
        if (resp->content_receiver && resp->content_receiver(buf, len)) {
            return 0;
        }
        resp->body.append(buf, len);
        return 0;
    }

    static int OnHeaderComplete(http_parser *p, const char *buf, size_t len) {return 0;}

    static int EmptyCB(http_parser *p) {return 0;}

    static int EmptyDataCB(http_parser *p, const char *buf, size_t len) {return 0;}

    // private members...
private:
    http_parser parser;
    http_parser_settings settings;
    std::string field;
    std::string value;

    unsigned char pre_state{0};
    size_t content_length_ = 0;
};

class Result {
public:
    Result(std::unique_ptr<Response> &&res, Error err,
           Headers &&request_headers = Headers{})
            : res_(std::move(res)), err_(err),
              request_headers_(std::move(request_headers)) {}
    Result(Result&& result)
        : res_(std::move(result.res_)), err_(result.err_), request_headers_(std::move(request_headers_)){}
    // Response
    operator bool() const { return res_ != nullptr; }
    bool operator==(std::nullptr_t) const { return res_ == nullptr; }
    bool operator!=(std::nullptr_t) const { return res_ != nullptr; }
    const Response &value() const { return *res_; }
    Response &value() { return *res_; }
    const Response &operator*() const { return *res_; }
    Response &operator*() { return *res_; }
    const Response *operator->() const { return res_.get(); }
    Response *operator->() { return res_.get(); }

    // Error
    Error error() const { return err_; }

    // Request Headers
    bool has_request_header(const std::string &key) const;
    std::string get_request_header_value(const std::string &key,
                                         size_t id = 0) const;
    std::string get_response_header_value(const std::string &key, size_t id = 0) const;

private:
    std::unique_ptr<Response> res_;
    Error err_;
    Headers request_headers_;
};

typedef std::shared_ptr<Result> ResultPtr;

class CoinClient {
public:
    // use ip and port
    explicit CoinClient(const std::string &host, int port);

    explicit CoinClient(const std::string &host, int port,
                        const std::string &ca_cert_path,
                        const std::string &client_cert_path,
                        const std::string &client_key_path,
                        bool must_ssl = false);

    // use http URL
    explicit CoinClient(const std::string &scheme_host_port);

    explicit CoinClient(const std::string &scheme_host_port,
                        const std::string &ca_cert_path,
                        const std::string &client_cert_path,
                        const std::string &client_key_path);

    virtual ~CoinClient();

    WorkerPool* get_workers();

    class Session {
    public:
        Session(ClientConnection* conn, CoinClient* client) : connection_(conn), client_(client) {}

        //Result Get(const std::string &path);
        //Result Get(const std::string &path, const Headers &headers);
        //Result Get(const std::string &path, ContentReceiver content_receiver);
        Result Get(const std::string &path, const Headers &headers, ContentReceiver content_receiver);
        //Result Get(const std::string &path, const Params &params, const Headers &headers);
        Result Get(const std::string &path, const Params &params, const Headers &headers, ContentReceiver content_receiver);
        Future<Error, ResultPtr> Future_Get(const std::string &path, const Params &params,
                                            const Headers &headers, ContentReceiver content_receiver = nullptr);

        Result Head(const std::string &path);
        Result Head(const std::string &path, const Headers &headers);

        Result Post(const std::string &path);
        Result Post(const std::string &path, const Headers &headers);
        Result Post(const std::string &path, const std::string &body,
                    const std::string &content_type);
        Result Post(const std::string &path, const Headers &headers,
                    const std::string &body, const std::string &content_type);
        Result Post(const std::string &path, const Headers &headers,
                    const MultipartFormDataItems &items,
                    const std::string &boundary);
        Result Post(const std::string &path,
                    const MultipartFormDataItems &items);
        Result Post(const std::string &path, const Headers &headers,
                    const MultipartFormDataItems &items);
        Result Post(const std::string &path, const Params &params);
        Result Post(const std::string &path, const Headers &headers,
                    const Params &params);
        Future<Error, ResultPtr> Future_Post(const std::string &path, const Headers &headers,
                                             const std::string &body, const std::string &content_type);


        //Result Put(const std::string &path);
        Result Put(const std::string &path, const char *body, size_t content_length,
                   const std::string &content_type);
        Result Put(const std::string &path, const Headers &headers, const char *body,
                   size_t content_length, const std::string &content_type);
        Result Put(const std::string &path, const std::string &body,
                   const std::string &content_type);
        Result Put(const std::string &path, const Headers &headers,
                   const std::string &body, const std::string &content_type);
        Result Put(const std::string &path, const Headers &headers,
                   const MultipartFormDataItems &items,
                   const std::string &boundary);
        Result Put(const std::string &path, const Headers &headers,
                   const MultipartFormDataItems &items);
        Result Put(const std::string &path, const Headers &headers,
                   const Params &params);

        Result Patch(const std::string &path);
        Result Patch(const std::string &path, const char *body, size_t content_length,
                     const std::string &content_type);
        Result Patch(const std::string &path, const Headers &headers,
                     const char *body, size_t content_length,
                     const std::string &content_type);
        Result Patch(const std::string &path, const std::string &body,
                     const std::string &content_type);
        Result Patch(const std::string &path, const Headers &headers,
                     const std::string &body, const std::string &content_type);

        Result Delete(const std::string &path);
        Result Delete(const std::string &path, const Headers &headers);
        Result Delete(const std::string &path, const char *body,
                      size_t content_length, const std::string &content_type);
        Result Delete(const std::string &path, const Headers &headers,
                      const char *body, size_t content_length,
                      const std::string &content_type);
        Result Delete(const std::string &path, const std::string &body,
                      const std::string &content_type);
        Result Delete(const std::string &path, const Headers &headers,
                      const std::string &body, const std::string &content_type);

        Result Options(const std::string &path);
        Result Options(const std::string &path, const Headers &headers);

        Result WebSocket(const std::string& path, const Params &params, const Headers &headers, WebSocketCallback callback);

        ClientConnection* connection() { return connection_.get(); }

        bool check_ws_callback(WebSocketCallback& callback);
    private:
        std::unique_ptr<ClientConnection> connection_;
        CoinClient* client_;

    };

    std::unique_ptr<Session> new_session();

    void set_default_headers(Headers headers) {default_headers_ = std::move(headers);}

    void set_connection_timeout(int32_t timeout) {connection_timeout_msec_ = timeout;}

    void set_read_timeout(int32_t timeout) {read_timeout_msec_ = timeout;}

    void set_write_timeout(int32_t timeout){write_timeout_msec_ = timeout;}

    void set_basic_auth(const std::string &username, const std::string &password) {
        basic_auth_username_ = username;
        basic_auth_password_ = password;
    }

    void set_bearer_token_auth(const std::string &token) {bearer_token_auth_token_ = token;}

    void set_keep_alive(bool on) { keep_alive_ = on; }

    void set_follow_location(bool on) { follow_location_ = on; }

    void set_url_encode(bool on) { url_encode_ = on; }

    void set_compress(bool on) { compress_ = on; }

    void set_decompress(bool on) { decompress_ = on; }

private:

    Future<Error, bool> send(Session* session, Request &req, Response &res, Error &error);

    bool recv_and_parse(Session* session, Buffer* buffer, Response &res);

    bool process_request(Session* session, Request &req, Response &res,
                         bool close_connection, Error &error);

    std::unique_ptr<Response> send_with_content_provider(
            Session* session,
            Request &req, const std::string& body, size_t content_length,
            const std::string &content_type, Error &error);

    Result send_with_content_provider(
            Session* session,
            const std::string &method, const std::string &path,
            const Headers &headers, const std::string& body, size_t content_length,
            const std::string &content_type);

    Result send_content(Session* session, Request &&req);
    bool handle_request(Session* session, Request &req, Response &res, bool close_connection, Error &error);
    bool write_request(Session* session, Request &req, bool close_connection, Error &error);
    bool redirect(Request &req, Response &res, Error &error);
    std::string adjust_host_string(const std::string &host) const;

    bool is_ssl() const { return false;}

private:
    std::unique_ptr<ApplicationClient> application_client_;
    // Socket endpoint information
    std::string host_;
    int port_;
    std::string host_and_port_;
    // Default headers
    Headers default_headers_;
    // Settings
    std::string client_cert_path_;
    std::string client_key_path_;
    std::string ca_cert_path_;
    int32_t connection_timeout_msec_ = 1000;
    int32_t read_timeout_msec_ = 5000;
    int32_t write_timeout_msec_ = 5000;
    std::string basic_auth_username_;
    std::string basic_auth_password_;
    std::string bearer_token_auth_token_;
    bool keep_alive_ = false;
    bool follow_location_ = false;
    bool url_encode_ = true;
    bool compress_ = false;
    bool decompress_ = false;

    std::unique_ptr<WorkerPool> workers_;
    acl::fiber_mutex workers_mutex_;
};

}}