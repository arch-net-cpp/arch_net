#include <netinet/tcp.h>
#include "socket.h"

namespace arch_net {

int socket(int domain, int type, int protocol) {
    auto fd = ::socket(domain, type, protocol);
    if (fd == INVALID_SOCKET) {
        printf("create socket error, %s\r\n", acl_fiber_last_serror());
        getchar();
        exit (1);
    }
    return fd;
}
int socket6(int type) {
    return arch_net::socket(AF_INET6, type);
}

int unix_socket(int type){
    return arch_net::socket(AF_UNIX, type, 0);
}

int udp_socket() {
    return arch_net::socket(AF_INET, SOCK_DGRAM, 0);
}

int udp6_socket() {
    return arch_net::socket6(SOCK_DGRAM);
}

int close(int fd) {
    acl_fiber_close(fd);
    return 0;
}

int listen(int fd, const char *ip, int port) {

    struct sockaddr_in sa;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(port);
    sa.sin_addr.s_addr = inet_addr(ip);

    int ret = 0;
    int reuse = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse , sizeof(int));
    if (ret < 0) {
        LOG(ERROR) << "setsockopt SO_REUSEADDR error";
        return ERR;
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const void *)&reuse , sizeof(int));
    if (ret < 0) {
        LOG(ERROR) << "setsockopt SO_REUSEPORT error";
        return ERR;
    }

    if (bind(fd, (struct sockaddr *) &sa, sizeof(struct sockaddr)) < 0) {
        LOG(ERROR) << "bind error " << acl_fiber_last_serror();
        return ERR;
    }

    if (acl_fiber_listen(fd, 1024) < 0) {
        LOG(ERROR) << "listen error " << acl_fiber_last_serror();
        return ERR;
    }

    return fd;
}

int listen(int fd, const std::string& path) {

    struct sockaddr_un addr;

    if (unlink (path.c_str()) == -1 && errno != ENOENT) {
        LOG(ERROR) << "Removing socket file failed";
        return ERR;
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);

    if (bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        LOG(ERROR) << "bind error " << acl_fiber_last_serror();
        return ERR;
    }
    if (acl_fiber_listen(fd, 1024) < 0) {
        LOG(ERROR) << "listen error " << acl_fiber_last_serror();
        return ERR;
    }

    return OK;
}

int udp_server(const char *ip, int port) {

    int listenfd = udp_socket();

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (::bind(listenfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
        LOG(ERROR) << "bind error " << acl_fiber_last_serror();
        return ERR;
    }

    int sockopt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void* )&sockopt, sizeof(sockopt)) < 0) {
        LOG(ERROR) << "setsockopt SO_REUSEADDR error";
        return ERR;
    }

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (void* )&sockopt, sizeof(sockopt)) < 0) {
        LOG(ERROR) << "setsockopt SO_REUSEADDR error";
        return ERR;
    }

    return listenfd;
}


int accept(int fd) {
    int cfd;
    struct sockaddr_in sa;
    int len = sizeof(sa);

    cfd = acl_fiber_accept(fd, (struct sockaddr *)& sa, (socklen_t *)& len);
    return cfd;
}

int connect(int fd, const char *ip, int port, int conn_timeout) {

    struct sockaddr_in sa;
    socklen_t len = (socklen_t) sizeof(sa);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    sa.sin_addr.s_addr = inet_addr(ip);

    if (conn_timeout > 0) {
        set_non_blocking(fd);
    }
    int ret = acl_fiber_connect(fd, (const struct sockaddr *) &sa, len);
    if (ret == 0) {
        return OK;
    }
    if (acl_fiber_last_error() != EINPROGRESS) {
        acl_fiber_close(fd);
        LOG(ERROR) << "connect to ip:port "<< ip <<":"<<port << " error: "<< acl_fiber_last_serror();
        return INVALID_SOCKET;
    }
    if (wait_fd_write_timeout(fd, conn_timeout) <= 0) {
        acl_fiber_close(fd);
        LOG(ERROR) << "connect to ip:port "<< ip <<":"<<port << " error: "<< acl_fiber_last_serror();
        return INVALID_SOCKET;
    }
    return OK;
}

int connect(int fd, struct sockaddr_storage& sock_addr, int conn_timeout) {
    socklen_t len = (socklen_t) sizeof(struct sockaddr);
    if (conn_timeout > 0) {
        set_non_blocking(fd);
    }
    int ret = acl_fiber_connect(fd, sockaddr_cast(&sock_addr), len);
    if (ret == 0) {
        return OK;
    }
    if (acl_fiber_last_error() != EINPROGRESS) {
        acl_fiber_close(fd);
        LOG(ERROR) << "connect to ip:port "<< ToIPPort(&sock_addr) << " error: "<< acl_fiber_last_serror();
        return INVALID_SOCKET;
    }
    if (wait_fd_write_timeout(fd, conn_timeout) <= 0) {
        acl_fiber_close(fd);
        LOG(ERROR) << "connect to ip:port "<< ToIPPort(&sock_addr) << " error: "<< acl_fiber_last_serror();
        return INVALID_SOCKET;
    }
    return OK;
}


int connect(int fd, const std::string& path) {
    struct sockaddr_un name;
    socklen_t len = (socklen_t) sizeof(name);

    int count = strlen(path.c_str());

    memset(&name, 0, sizeof(name));
    memcpy(name.sun_path, path.c_str(), count + 1);
#ifndef __linux__
    name.sun_len = 0;
#endif
    name.sun_family = AF_UNIX;

    if (acl_fiber_connect(fd, (const struct sockaddr *) &name, len) < 0) {
        acl_fiber_close(fd);
        LOG(ERROR) << "connect to UNIX Socket "<< path << " error: "<< acl_fiber_last_serror();
        return INVALID_SOCKET;
    }

    return OK;
}

void connect_udp(const std::string& ip, int p, struct sockaddr_in& server) {
    unsigned short port;
    port = htons(p);
    /* Set up the server name */
    server.sin_family      = AF_INET;            /* Internet Domain    */
    server.sin_port        = port;               /* Server Port        */
    server.sin_addr.s_addr = inet_addr(ip.c_str());
}

ssize_t read(int fd, void *buf, size_t count, int timeout) {
    if (timeout > 0 && wait_fd_read_timeout(fd, timeout) <= 0) {
        return RecvTimeout;
    }
    return acl_fiber_read(fd, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt, int timeout) {
    if (timeout >0 && wait_fd_read_timeout(fd, timeout) <= 0) {
        return RecvTimeout;
    }
    return acl_fiber_readv(fd, iov, iovcnt);
}

ssize_t recvfrom(int sock, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
    return acl_fiber_recvfrom(sock, buf, len, flags, src_addr, addrlen);
}

ssize_t write(int fd, const void *buf, size_t count, int timeout) {
    if (timeout > 0 && wait_fd_write_timeout(fd, timeout) <= 0) {
        return SendTimeout;
    }
    return acl_fiber_write(fd, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt, int timeout) {
    if (timeout > 0 && wait_fd_write_timeout(fd, timeout) <=0) {
        return SendTimeout;
    }
    return acl_fiber_writev(fd, iov, iovcnt);
}

ssize_t sendto(int sock, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
    return acl_fiber_sendto(sock, buf, len, flags, dest_addr, addrlen);
}

std::string transfer_ip_port(const struct sockaddr_in& addr) {
    std::string ip_port;
    ip_port.append(inet_ntoa(addr.sin_addr)).append(":").append(std::to_string(ntohs(addr.sin_port)));
    return ip_port;
}

int set_non_blocking(int fd) {
    int   flags;
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

int wait_fd_write_timeout(int fd, int timeout) {
    struct pollfd fds;
    fds.events = POLLOUT;
    fds.revents = 0;
    fds.fd = fd;

    for (;;) {
        switch (acl_fiber_poll(&fds, 1,  timeout)) {
        case -1:
            return -1;
        case 0:
            // timeout
            return -1;
        default:
            if (fds.revents & POLLNVAL) {
                // invalid
                return -1;
            }
            if ((fds.revents & (POLLHUP | POLLERR))) {
                // conn refused
                return -1;
            }
            if (fds.revents & POLLOUT) {
                return 1;
            }
            return 0;
        }
    }
}

int wait_fd_read_timeout(int fd, int timeout) {
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN | POLLHUP | POLLERR | POLLPRI;
    fds.revents = 0;
    for (;;) {
        switch (acl_fiber_poll(&fds, 1, timeout)) {
        case -1:
            return -1;
        case 0:
            // timeout
            return 0;
        default:
            if ((fds.revents & POLLIN)) {
                return 1;
            } else if (fds.revents & (POLLHUP | POLLERR)) {
                return 1;
            } else {
                return 0;
            }
        }
    }
}

int wait_fds_readable(const std::vector<int>& fds, std::vector<int>& readable_fds, int timeout) {
    std::vector<struct pollfd> pfds;

    for (auto & fd : fds) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN | POLLHUP | POLLERR | POLLPRI;
        pfd.revents = 0;
        pfds.emplace_back(pfd);
    }


    int n = acl_fiber_poll(&pfds[0], pfds.size(), timeout);
    if (n <= 0) {
        return n;
    }
    for (size_t i = 0; i < pfds.size(); ++i) {
        // 事件可读
        if ((pfds[i].revents & POLLIN)) {
            readable_fds.emplace_back(pfds[i].fd);
        } else if (pfds[i].revents & (POLLHUP | POLLERR)) {
            readable_fds.emplace_back(pfds[i].fd);
        } else {
            continue;
        }
    }
    return 1;
}


bool ParseFromIPPort(const std::string& address, struct sockaddr_storage& ss, bool& is_v6) {
    memset(&ss, 0, sizeof(ss));
    std::string host;
    int port;
    if (!SplitHostPort(address, host, port)) {
        return false;
    }

    return ParseFromIPPort(host, port, ss, is_v6);
}

bool ParseFromIPPort(const std::string& host, int port, struct sockaddr_storage& ss, bool& is_v6) {
    memset(&ss, 0, sizeof(ss));
    short family = AF_INET;
    auto index = host.find(':');
    // IPV6
    if (index != std::string::npos) {
        family = AF_INET6;
        struct sockaddr_in6* addr = sockaddr_in6_cast(&ss);
        int rc = ::inet_pton(family, host.data(), &addr->sin6_addr);
        if (rc <= 0) {
            return false;
        }
        addr->sin6_family = family;
        addr->sin6_port = htons(port);
        is_v6 = true;
        return true;
    }
    // IPV4
    struct sockaddr_in* addr = sockaddr_in_cast(&ss);
    int rc = ::inet_pton(family, host.data(), &addr->sin_addr);
    if (rc <= 0) {
        return false;
    }
    addr->sin_family = family;
    addr->sin_port = htons(port);
    is_v6 = false;
    return true;
}

bool SplitHostPort(const std::string& a, std::string& host, int& port) {
    if (a.empty()) {
        return false;
    }

    size_t index = a.rfind(':');
    if (index == std::string::npos) {
        return false;
    }

    if (index == a.size() - 1) {
        return false;
    }

    port = std::atoi(&a[index + 1]);

    host = std::string(a.data(), index);
    if (host[0] == '[') {
        if (*host.rbegin() != ']') {
            //LOG_ERROR << "Address specified error <" << address << ">. '[' ']' is not pair.";
            return false;
        }

        // trim the leading '[' and trail ']'
        host = std::string(host.data() + 1, host.size() - 2);
    }

    // Compatible with "fe80::886a:49f3:20f3:add2]:80"
    if (*host.rbegin() == ']') {
        // trim the trail ']'
        host = std::string(host.data(), host.size() - 1);
    }

    return true;
}


std::string ToIPPort(const struct sockaddr_storage* ss) {
    std::string saddr;
    int port = 0;

    if (ss->ss_family == AF_INET) {
        struct sockaddr_in* addr4 = const_cast<struct sockaddr_in*>(sockaddr_in_cast(ss));
        char buf[INET_ADDRSTRLEN] = {};
        const char* addr = ::inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);

        if (addr) {
            saddr = addr;
        }

        port = ntohs(addr4->sin_port);
    } else if (ss->ss_family == AF_INET6) {
        struct sockaddr_in6* addr6 = const_cast<struct sockaddr_in6*>(sockaddr_in6_cast(ss));
        char buf[INET6_ADDRSTRLEN] = {};
        const char* addr = inet_ntop(ss->ss_family, &addr6->sin6_addr, buf, INET6_ADDRSTRLEN);

        if (addr) {
            saddr = std::string("[") + addr + "]";
        }

        port = ntohs(addr6->sin6_port);
    } else {
        return "";
    }

    if (!saddr.empty()) {
        saddr.append(":", 1).append(std::to_string(port));
    }

    return saddr;
}

std::string ToIPPort(const struct sockaddr* ss) {
    return ToIPPort(sockaddr_storage_cast(ss));
}

std::string ToIPPort(const struct sockaddr_in* ss) {
    return ToIPPort(sockaddr_storage_cast(ss));
}

std::string ToIP(const struct sockaddr* s) {
    auto ss = sockaddr_storage_cast(s);
    if (ss->ss_family == AF_INET) {
        struct sockaddr_in* addr4 = const_cast<struct sockaddr_in*>(sockaddr_in_cast(ss));
        char buf[INET_ADDRSTRLEN] = {};
        const char* addr = ::inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);
        if (addr) {
            return {addr};
        }
    } else if (ss->ss_family == AF_INET6) {
        struct sockaddr_in6* addr6 = const_cast<struct sockaddr_in6*>(sockaddr_in6_cast(ss));
        char buf[INET6_ADDRSTRLEN] = {};
        const char* addr = ::inet_ntop(ss->ss_family, &addr6->sin6_addr, buf, INET6_ADDRSTRLEN);
        if (addr) {
            return {addr};
        }
    } else {

    }
    return "";
}

void dns_resolve(const std::string& host_name, std::vector<std::string>& addrs) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; /* v4 or v6 is fine. */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    /* Look up the hostname. */
    struct addrinfo* answer = nullptr;
    int err = acl_fiber_getaddrinfo(host_name.c_str(), nullptr, &hints, &answer);
    if (err != 0) {
        acl_fiber_freeaddrinfo(answer);
        return;
    }
    for (struct addrinfo* rp = answer; rp != nullptr; rp = rp->ai_next) {
        auto addr = ToIP(rp->ai_addr);
        if (!addr.empty()) {
            addrs.emplace_back(addr);
        }
    }
    acl_fiber_freeaddrinfo(answer);
}

int InitUnixSocketServer(std::string & sock_path){
    int serrno = 0;

    /*Creat Unix domain Socket*/
    int fd = unix_socket();
    if (fd == -1) {
        serrno = errno;
        return INVALID_SOCKET;
    }

    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path.c_str());

    unlink(sock_path.c_str());
    if( ::bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0){
        serrno = errno;
        LOG(ERROR) << "socket error " << strerror(serrno);
        goto out;
    }

    if(acl_fiber_listen(fd, 5) < 0){
        serrno = errno;
        LOG(ERROR) << "socket error " << strerror(serrno);
        goto out;
    }

    return fd;

    out:
    close(fd);
    return INVALID_SOCKET;
}

int InitUnixSocketClient(std::string & sock_path){
    int serrno = 0;

    int fd = unix_socket();
    if (fd == -1) {
        serrno = errno;
        LOG(ERROR) << "socket error " << strerror(serrno);
        return INVALID_SOCKET;
    }

    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path.c_str());

    if(::acl_fiber_connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        serrno = errno;
        LOG(ERROR) << "socket error " << strerror(serrno);
        goto out;
    }

    return fd;

    out:
    close(fd);
    return INVALID_SOCKET;
}

void SetKeepAlive(int fd, bool on){
    int optval = on ? 1 : 0;
    int rc = ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    if (rc != 0) {
        int serrno = errno;
        LOG(ERROR) << "setsockopt(SO_KEEPALIVE) failed, errno=" << serrno << " " << strerror(serrno);
    }
}

void SetReuseAddr(int fd){
    int optval = 1;
    int rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    if (rc != 0) {
        int serrno = errno;
        LOG(ERROR) << "setsockopt(SO_REUSEADDR) failed, errno=" << serrno << " " << strerror(serrno);
    }
}

void SetReusePort(int fd){
#ifdef SO_REUSEPORT
    int optval = 1;
    int rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    LOG(ERROR) << "setsockopt SO_REUSEPORT fd=" << fd << " rc=" << rc;
    if (rc != 0) {
        int serrno = errno;
        LOG(ERROR) << "setsockopt(SO_REUSEPORT) failed, errno=" << serrno << " " << strerror(serrno);
    }
#endif
}

void SetTCPNoDelay(int fd, bool on){
    int optval = on ? 1 : 0;
    int rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    if (rc != 0) {
        int serrno = errno;
        LOG(ERROR) << "setsockopt(TCP_NODELAY) failed, errno=" << serrno << " " << strerror(serrno);
    }
}

void SetTimeout(int fd, uint32_t timeout_ms){
    struct timeval tv;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    assert(ret == 0);
    if (ret != 0) {
        int err = errno;
        LOG(ERROR) << "setsockopt SO_RCVTIMEO ERROR " << err << strerror(err);
    }
}

}