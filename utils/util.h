#pragma once
#include "singleton.h"
#include "iostream"
#include "slice.h"

#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/time.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>

class Random : public Singleton<Random> {
public:
    int Rand() {
        return rand();
    }
    Random() {
        srand((unsigned)time(nullptr));
    }
};

static const std::string alphabet = "0123456789-=_+qwertyuiop[]asdfghjkl;'zxcvbnm,./";
// get current date/time, format is YYYY-MM-DD HH:mm:ss

static std::string get_current_datetime(time_t ts) {
    if (ts == 0) {
        ts = time(nullptr);
    }
    struct tm tm{};
    localtime_r(&ts, &tm);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

static int get_hour(time_t ts) {
    if (ts == 0) {
        ts = time(nullptr);
    }
    struct tm tm{};
    localtime_r(&ts, &tm);
    return tm.tm_hour;
}

static std::string gen_random_str(int num = 10) {
    std::string res;
    res.resize(num);
    for (int i = 0; i < num; i++) {
        res[i] = alphabet[Random::getInstance().Rand() % alphabet.size()];
    }
    return res;
}

static void string_split(const std::string& str, char delim, std::vector<std::string>& result) {
    size_t start = std::string::npos;
    for (size_t i = 0; i <= str.length(); ++i) {
        if (start == std::string::npos) {
            start = i;
        }
        if (i == str.length() || str[i] == delim) {
            // skip if the start point itself is a delimiter
            result.emplace_back(str.c_str() + start, str.c_str() + i);
            start = std::string::npos;
        }
    }
}

static std::string string_to_lower(std::string src) {
    if (src.empty()) return "";
    std::string lower;
    lower.resize(src.size());
    transform(src.begin(), src.end(), lower.begin(), ::tolower);
    return lower;
}

static std::string string_to_upper(std::string src) {
    if (src.empty()) return "";
    std::string upper;
    upper.resize(src.size());
    transform(src.begin(), src.end(), upper.begin(), ::toupper);
    return upper;
}

static void trim_path_spaces(std::string& s) {
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
}

static std::string trim_prefix_slash(std::string s) {
    std::function<bool(char)> is_slash = [](char x) { return x == '/'; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(is_slash)));
    return s;
}

// TODO: optimize regex matche
static bool is_str_match_regex(const std::string& str, const std::string& regex) {
    if (regex.empty()) {
        return false;
    }
    auto pattern = std::regex(regex);
    bool matched = std::regex_match(str, pattern);
    return matched;
}

static void mixin(std::unordered_map<std::string, std::string>& a,
           std::unordered_map<std::string, std::string>& b) {
    for (auto& it : b) {
        a[it.first] = it.second;
    }
}

static std::string& string_replace_recursion(std::string& str, const std::string& old_value,
                                      const std::string& new_value) {
    while (true) {
        std::string::size_type pos(0);
        if ((pos = str.find(old_value)) != std::string::npos)
            str.replace(pos, old_value.length(), new_value);
        else
            break;
    }
    return str;
}

static std::string& string_replace(std::string& str, const std::string& old_value,
                            const std::string& new_value) {
    for (std::string::size_type pos(0); pos != std::string::npos; pos += new_value.length()) {
        if ((pos = str.find(old_value, pos)) != std::string::npos)
            str.replace(pos, old_value.length(), new_value);
        else
            break;
    }
    return str;
}

static void slim_path(std::string path, std::string& final_path) {
    if (path.empty()) {
        final_path = path;
    }
    final_path = string_replace_recursion(path, "//", "/");
}

/* get system time */
static inline void itimeofday(long *sec, long *usec) {
    struct timeval time{};
    gettimeofday(&time, nullptr);
    if (sec) *sec = time.tv_sec;
    if (usec) *usec = time.tv_usec;
}

/* get clock in millisecond 64 */
static inline int64_t iclock64() {
    long s, u;
    int64_t value;
    itimeofday(&s, &u);
    value = ((int64_t)s) * 1000 + (u / 1000);
    return value;
}

static inline uint32_t iclock() {
    struct timeval time_now{};
    gettimeofday(&time_now, nullptr);
    time_t msecs_time = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    return msecs_time;
}


// url encode/decode
/* Converts a hex character to its integer value */
static inline char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
static inline char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
static inline std::string url_encode(const std::string& str) {
    auto *pstr = str.c_str();
    std::string buf;
    buf.resize(str.length() * 3 + 1);
    auto *pbuf = &buf[0];
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            *pbuf++ = *pstr;
        else if (*pstr == ' ')
            *pbuf++ = '+';
        else
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
static inline std::string url_decode(const std::string& str) {
    auto *pstr = str.c_str();
    std::string buf;
    buf.resize(str.length());
    auto *pbuf = &buf[0];
    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') {
            *pbuf++ = ' ';
        } else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

template<class InputIterator>
static std::string container_to_string(InputIterator first, InputIterator last) {
    std::stringstream s_stream;
    typedef typename InputIterator::value_type T;
    std::copy(first, last, std::ostream_iterator<T>(s_stream, "|"));
    return s_stream.str();
}