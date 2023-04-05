#pragma once
#include "iostream"
#include "map"
#include <sstream>
#include <iomanip>
#include <random>
#include <zlib.h>

namespace arch_net {namespace coin{

struct MultipartFormData {
    std::string name;
    std::string content;
    std::string filename;
    std::string content_type;
};

using MultipartFormDataItems = std::vector<MultipartFormData>;
using MultipartFormDataMap = std::multimap<std::string, MultipartFormData>;

struct cmp {
    bool operator()(const std::string &s1, const std::string &s2) const {
        return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                            [](unsigned char c1, unsigned char c2) {
                                                return ::tolower(c1) < ::tolower(c2);
                                            });
    }
};

using Headers = std::multimap<std::string, std::string, cmp>;

inline bool has_header(const Headers &headers, const std::string &key) {
    return headers.find(key) != headers.end();
}

inline const char *get_header_value(const Headers &headers,
                                    const std::string &key,
                                    const char *def) {
    auto rng = headers.equal_range(key);
    auto it = rng.first;
    if (it != rng.second) { return it->second.c_str(); }
    return def;
}

inline bool is_hex(char c, int &v) {
    if (0x20 <= c && isdigit(c)) {
        v = c - '0';
        return true;
    } else if ('A' <= c && c <= 'F') {
        v = c - 'A' + 10;
        return true;
    } else if ('a' <= c && c <= 'f') {
        v = c - 'a' + 10;
        return true;
    }
    return false;
}

inline bool has_crlf(const std::string &s) {
    auto p = s.c_str();
    while (*p) {
        if (*p == '\r' || *p == '\n') { return true; }
        p++;
    }
    return false;
}

inline bool from_hex_to_i(const std::string &s, size_t i, size_t cnt,
                          int &val) {
    if (i >= s.size()) { return false; }

    val = 0;
    for (; cnt; i++, cnt--) {
        if (!s[i]) { return false; }
        int v = 0;
        if (is_hex(s[i], v)) {
            val = val * 16 + v;
        } else {
            return false;
        }
    }
    return true;
}

inline std::string from_i_to_hex(size_t n) {
    const char *charset = "0123456789abcdef";
    std::string ret;
    do {
        ret = charset[n & 15] + ret;
        n >>= 4;
    } while (n > 0);
    return ret;
}

inline size_t to_utf8(int code, char *buff) {
    if (code < 0x0080) {
        buff[0] = (code & 0x7F);
        return 1;
    } else if (code < 0x0800) {
        buff[0] = static_cast<char>(0xC0 | ((code >> 6) & 0x1F));
        buff[1] = static_cast<char>(0x80 | (code & 0x3F));
        return 2;
    } else if (code < 0xD800) {
        buff[0] = static_cast<char>(0xE0 | ((code >> 12) & 0xF));
        buff[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        buff[2] = static_cast<char>(0x80 | (code & 0x3F));
        return 3;
    } else if (code < 0xE000) { // D800 - DFFF is invalid...
        return 0;
    } else if (code < 0x10000) {
        buff[0] = static_cast<char>(0xE0 | ((code >> 12) & 0xF));
        buff[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        buff[2] = static_cast<char>(0x80 | (code & 0x3F));
        return 3;
    } else if (code < 0x110000) {
        buff[0] = static_cast<char>(0xF0 | ((code >> 18) & 0x7));
        buff[1] = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        buff[2] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        buff[3] = static_cast<char>(0x80 | (code & 0x3F));
        return 4;
    }

    // NOTREACHED
    return 0;
}

inline std::string encode_query_param(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (auto c : value) {
        if (std::isalnum(static_cast<uint8_t>(c)) || c == '-' || c == '_' ||
            c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' ||
            c == ')') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

inline std::string params_to_query_str(const std::multimap<std::string, std::string> &params) {
    std::string query;

    for (auto it = params.begin(); it != params.end(); ++it) {
        if (it != params.begin()) { query += "&"; }
        query += it->first;
        query += "=";
        query += encode_query_param(it->second);
    }
    return query;
}

using Params = std::multimap<std::string, std::string>;
inline std::string append_query_params(const std::string &path,
                                       const Params &params) {
    std::string path_with_query = path;
    const static std::regex re("[^?]+\\?.*");
    auto delm = std::regex_match(path, re) ? '&' : '?';
    path_with_query += delm + params_to_query_str(params);
    return path_with_query;
}

inline std::string
serialize_multipart_formdata_get_content_type(const std::string &boundary) {
    return "multipart/form-data; boundary=" + boundary;
}

inline std::string make_multipart_data_boundary() {
    static const char data[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    // std::random_device might actually be deterministic on some
    // platforms, but due to lack of support in the c++ standard library,
    // doing better requires either some ugly hacks or breaking portability.
    std::random_device seed_gen;

    // Request 128 bits of entropy for initialization
    std::seed_seq seed_sequence{seed_gen(), seed_gen(), seed_gen(), seed_gen()};
    std::mt19937 engine(seed_sequence);

    std::string result = "--cpp-httplib-multipart-data-";

    for (auto i = 0; i < 16; i++) {
        result += data[engine() % (sizeof(data) - 1)];
    }

    return result;
}

inline bool is_multipart_boundary_chars_valid(const std::string &boundary) {
    auto valid = true;
    for (size_t i = 0; i < boundary.size(); i++) {
        auto c = boundary[i];
        if (!std::isalnum(c) && c != '-' && c != '_') {
            valid = false;
            break;
        }
    }
    return valid;
}

// NOTE: This code came up with the following stackoverflow post:
// https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
inline std::string base64_encode(const std::string &in) {
    static const auto lookup =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(in.size());

    int val = 0;
    int valb = -6;

    for (auto c : in) {
        val = (val << 8) + static_cast<uint8_t>(c);
        valb += 8;
        while (valb >= 0) {
            out.push_back(lookup[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) { out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]); }

    while (out.size() % 4) {
        out.push_back('=');
    }

    return out;
}

inline std::pair<std::string, std::string>
make_basic_authentication_header(const std::string &username,
                                 const std::string &password, bool is_proxy) {
    auto field = "Basic " + base64_encode(username + ":" + password);
    auto key = is_proxy ? "Proxy-Authorization" : "Authorization";
    return std::make_pair(key, std::move(field));
}

inline std::pair<std::string, std::string>
make_bearer_token_authentication_header(const std::string &token,
                                        bool is_proxy = false) {
    auto field = "Bearer " + token;
    auto key = is_proxy ? "Proxy-Authorization" : "Authorization";
    return std::make_pair(key, std::move(field));
}


template <typename T>
inline std::string
serialize_multipart_formdata_item_begin(const T &item,
                                        const std::string &boundary) {
    std::string body = "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + item.name + "\"";
    if (!item.filename.empty()) {
        body += "; filename=\"" + item.filename + "\"";
    }
    body += "\r\n";
    if (!item.content_type.empty()) {
        body += "Content-Type: " + item.content_type + "\r\n";
    }
    body += "\r\n";

    return body;
}

inline std::string serialize_multipart_formdata_item_end() { return "\r\n"; }

inline std::string
serialize_multipart_formdata_finish(const std::string &boundary) {
    return "--" + boundary + "--\r\n";
}


inline std::string
serialize_multipart_formdata(const MultipartFormDataItems &items,
                             const std::string &boundary, bool finish = true) {
    std::string body;

    for (const auto &item : items) {
        body += serialize_multipart_formdata_item_begin(item, boundary);
        body += item.content + serialize_multipart_formdata_item_end();
    }

    if (finish) body += serialize_multipart_formdata_finish(boundary);

    return body;
}


inline std::string encode_url(const std::string &s) {
    std::string result;
    result.reserve(s.size());

    for (size_t i = 0; s[i]; i++) {
        switch (s[i]) {
            case ' ': result += "%20"; break;
            case '+': result += "%2B"; break;
            case '\r': result += "%0D"; break;
            case '\n': result += "%0A"; break;
            case '\'': result += "%27"; break;
            case ',': result += "%2C"; break;
                // case ':': result += "%3A"; break; // ok? probably...
            case ';': result += "%3B"; break;
            default:
                auto c = static_cast<uint8_t>(s[i]);
                if (c >= 0x80) {
                    result += '%';
                    char hex[4];
                    auto len = snprintf(hex, sizeof(hex) - 1, "%02X", c);
                    assert(len == 2);
                    result.append(hex, static_cast<size_t>(len));
                } else {
                    result += s[i];
                }
                break;
        }
    }

    return result;
}

inline std::string decode_url(const std::string &s,
                              bool convert_plus_to_space) {
    std::string result;

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 1 < s.size()) {
            if (s[i + 1] == 'u') {
                int val = 0;
                if (from_hex_to_i(s, i + 2, 4, val)) {
                    // 4 digits Unicode codes
                    char buff[4];
                    size_t len = to_utf8(val, buff);
                    if (len > 0) { result.append(buff, len); }
                    i += 5; // 'u0000'
                } else {
                    result += s[i];
                }
            } else {
                int val = 0;
                if (from_hex_to_i(s, i + 1, 2, val)) {
                    // 2 digits hex codes
                    result += static_cast<char>(val);
                    i += 2; // '00'
                } else {
                    result += s[i];
                }
            }
        } else if (convert_plus_to_space && s[i] == '+') {
            result += ' ';
        } else {
            result += s[i];
        }
    }

    return result;
}

typedef std::function<bool(const char *data, size_t data_len)> CompressCallback;
class gzip_compressor {
public:
    gzip_compressor() {
        std::memset(&strm_, 0, sizeof(strm_));
        strm_.zalloc = Z_NULL;
        strm_.zfree = Z_NULL;
        strm_.opaque = Z_NULL;

        is_valid_ = deflateInit2(&strm_, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8,
                                 Z_DEFAULT_STRATEGY) == Z_OK;
    }
    ~gzip_compressor() { deflateEnd(&strm_); }

    bool compress(const char *data, size_t data_length, bool last,
                  CompressCallback callback) {
        assert(is_valid_);

        do {
            constexpr size_t max_avail_in =
                    (std::numeric_limits<decltype(strm_.avail_in)>::max)();

            strm_.avail_in = static_cast<decltype(strm_.avail_in)>(
                    (std::min)(data_length, max_avail_in));
            strm_.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(data));

            data_length -= strm_.avail_in;
            data += strm_.avail_in;

            auto flush = (last && data_length == 0) ? Z_FINISH : Z_NO_FLUSH;
            int ret = Z_OK;

            std::array<char, 8192> buff{};
            do {
                strm_.avail_out = static_cast<uInt>(buff.size());
                strm_.next_out = reinterpret_cast<Bytef *>(buff.data());

                ret = deflate(&strm_, flush);
                if (ret == Z_STREAM_ERROR) { return false; }

                if (!callback(buff.data(), buff.size() - strm_.avail_out)) {
                    return false;
                }
            } while (strm_.avail_out == 0);

            assert((flush == Z_FINISH && ret == Z_STREAM_END) ||
                   (flush == Z_NO_FLUSH && ret == Z_OK));
            assert(strm_.avail_in == 0);
        } while (data_length > 0);

        return true;
    }

private:
    bool is_valid_ = false;
    z_stream strm_;
};

}}