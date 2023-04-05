#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <fiber/libfiber.h>
#include <fiber/fiber_define.h>
#include "common.h"

namespace arch_net {
//UnderlyingSocketError
enum {
    SendTimeout = -1000,
    RecvTimeout = -1001,
};

struct SocketType {
    enum Type{
        IPV4,
        IPV6,
        UNIX,
    };
    SocketType(){ type = IPV4; }
    explicit SocketType(Type t) : type(t){}
    Type type;
};


int unix_socket(int type = SOCK_STREAM);

int socket(int domain = AF_INET, int type = SOCK_STREAM, int protocol = 0);
int socket6(int type = SOCK_STREAM);

int udp_socket();

int udp6_socket();

int close(int fd);

int listen(int fd, const char *ip, int port);

int listen(int fd, struct sockaddr_storage& sock_addr);

int listen(int fd, const std::string& path);

int udp_server(const char *ip, int port);

int accept(int fd);

int connect(int fd, const char *ip, int port, int conn_timeout=0);

int connect(int fd, struct sockaddr_storage& sock_addr, int conn_timeout=0);

int connect(int fd, const std::string& path);

void connect_udp(const std::string& ip, int port, struct sockaddr_in& addr);

ssize_t read(int fd, void *buf, size_t count, int timeout=0);

ssize_t readv(int fd, const struct iovec *iov, int iovcnt, int timeout=0);

ssize_t recvfrom(int sock, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen);

ssize_t write(int fd, const void *buf, size_t count, int timeout=0);

ssize_t writev(int fd, const struct iovec *iov, int iovcnt, int timeout=0);

ssize_t sendto(int sock, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen);

std::string transfer_ip_port(const struct sockaddr_in& addr);

int set_non_blocking(int fd);

int wait_fd_write_timeout(int fd, int timeout);
int wait_fd_read_timeout(int fd, int timeout);

int wait_fds_readable(const std::vector<int>& fds, std::vector<int>& readable_fds, int timeout);

template<typename To, typename From>
inline To implicit_cast(From const& f) {
    return f;
}

inline const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr) {
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

inline struct sockaddr* sockaddr_cast(struct sockaddr_in* addr) {
    return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

inline struct sockaddr* sockaddr_cast(struct sockaddr_storage* addr) {
    return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

inline const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr) {
    return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

inline struct sockaddr_in* sockaddr_in_cast(struct sockaddr* addr) {
    return static_cast<struct sockaddr_in*>(implicit_cast<void*>(addr));
}

inline struct sockaddr_in* sockaddr_in_cast(struct sockaddr_storage* addr) {
    return static_cast<struct sockaddr_in*>(implicit_cast<void*>(addr));
}

inline struct sockaddr_in6* sockaddr_in6_cast(struct sockaddr_storage* addr) {
    return static_cast<struct sockaddr_in6*>(implicit_cast<void*>(addr));
}

inline const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr_storage* addr) {
    return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

inline const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr_storage* addr) {
    return static_cast<const struct sockaddr_in6*>(implicit_cast<const void*>(addr));
}

inline const struct sockaddr_storage* sockaddr_storage_cast(const struct sockaddr* addr) {
    return static_cast<const struct sockaddr_storage*>(implicit_cast<const void*>(addr));
}

inline const struct sockaddr_storage* sockaddr_storage_cast(const struct sockaddr_in* addr) {
    return static_cast<const struct sockaddr_storage*>(implicit_cast<const void*>(addr));
}

inline const struct sockaddr_storage* sockaddr_storage_cast(const struct sockaddr_in6* addr) {
    return static_cast<const struct sockaddr_storage*>(implicit_cast<const void*>(addr));
}

bool ParseFromIPPort(const std::string& address, struct sockaddr_storage& ss, bool& is_v6);
bool ParseFromIPPort(const std::string& host, int port, struct sockaddr_storage& ss, bool& is_v6);
bool SplitHostPort(const std::string& a, std::string& host, int& port);
std::string ToIPPort(const struct sockaddr_storage* ss);

std::string ToIPPort(const struct sockaddr* ss);

std::string ToIPPort(const struct sockaddr_in* ss);
std::string ToIP(const struct sockaddr* ss);
void dns_resolve(const std::string& host_name, std::vector<std::string>& addrs);

int InitUnixSocketServer(std::string & sock_path);
int InitUnixSocketClient(std::string & sock_path);

}

