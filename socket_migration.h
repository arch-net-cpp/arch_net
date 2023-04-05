#pragma once
#include "common.h"
#include <csignal>
#include "socket.h"
#define MIGRATION_TIMEOUT_CYCLE 3000

namespace arch_net {

class MigrationEnv : public TNonCopyable{
public:
    std::string SVR_SOCKET_PATH;
    std::string SVR_PID_PATH;
    std::string MIG_SOCKET_PATH;

    MigrationEnv() = default;
};

class SocketMigration : public TNonCopyable {
public:

    typedef std::function<void()> Handler;

    SocketMigration() : old_svr_pid_(0){}
    // send listening socket to new server
    int send_socket(int socket);

    int recv_socket();

    int set_server_pid();

    int start();

    void set_fd(int fd){
        listen_fd_ = fd;
    }
private:

    int init_env();

    int get_old_pid();

    int set_new_pid();

    int check_server_run();

private:
    int listen_fd_ = 0;
    pid_t old_svr_pid_;
    MigrationEnv env_;
};

std::function<void(int)> callback_wrapper;
void callback_function(int value)
{
    callback_wrapper(value);
}
}