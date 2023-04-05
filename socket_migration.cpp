#include "socket_migration.h"

namespace arch_net {

int SocketMigration::start() {
    init_env();
    int sock_fd = 0;
    // if server already exist, first try to migrate it from old server
    if(check_server_run() == 0){
        sock_fd = recv_socket();
        if(sock_fd < 0) {
            LOG(ERROR) << "failed to migrate socket from old server";
            sock_fd = 0;
        }
    }

    callback_wrapper = [this](int) {
        LOG(INFO) << "recv socket migration signal";
        this->send_socket(this->listen_fd_);
    };

    struct sigaction sigIntHandler{};
    sigIntHandler.sa_handler = callback_function;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGUSR1, &sigIntHandler, NULL);

    return sock_fd;

}

int SocketMigration::check_server_run() {
    // if pid file is exist
    if (access(env_.SVR_PID_PATH.c_str(), F_OK) == 0) {
        // if pid is not zero, check pid is alive
        if (get_old_pid() == 0) {
            // if ret == 0, process is alive
            if(kill(old_svr_pid_, 0) == 0) {
                return 0;
            }
        }
    }

    return -1;
}

int SocketMigration::init_env() {
    // get absolute path
    const char * svr_socket_path = "//evsvr/evsvr_sock.sock"; //getenv("EVSVR_SOCKET_PATH");
    if(svr_socket_path != nullptr) {
        env_.SVR_SOCKET_PATH = std::string(svr_socket_path);
    }

    const char * svr_pid_path = "./evsvr_pid.txt";      //getenv("EVSVR_PID_PATH");
    if(svr_pid_path != nullptr) {
        env_.SVR_PID_PATH = std::string(svr_pid_path);
    }

    const char * mig_socket_path = "./mig_sock.sock";   //getenv("EVSVR_MIG_SOCKET_PATH");
    if(mig_socket_path != nullptr) {
        env_.MIG_SOCKET_PATH = std::string(mig_socket_path);
    }
    return 0;
}


int SocketMigration::recv_socket() {

    if(get_old_pid() < 0){
        return -1;
    }

    int svr_fd = InitUnixSocketServer(env_.MIG_SOCKET_PATH);
    if(svr_fd < 0){
        return -1;
    }

    struct msghdr msg;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_flags = 0;

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    struct iovec iov[1];
    char buf[1];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    int conn_fd = 0;
    int cycle;
    int ret;

    for( cycle = 0; cycle < MIGRATION_TIMEOUT_CYCLE; cycle++){
        if(cycle % 1000 == 0){            // send signal to old server
            kill(old_svr_pid_, SIGUSR1);
        }
        usleep(1000);
        if(conn_fd <= 0){
            conn_fd = accept(svr_fd);
            if(conn_fd <= 0){
                continue;
            }
        }
        auto n = acl_fiber_recvmsg(conn_fd, &msg, 0);
        if(n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)){
            close(conn_fd);
            conn_fd = 0;
            continue;
        }
        if(n < 0){
            continue;
        }

        struct cmsghdr *cmptr;
        if((cmptr = CMSG_FIRSTHDR(&msg)) == nullptr || cmptr->cmsg_len != CMSG_LEN(sizeof(int))
           || cmptr->cmsg_level != SOL_SOCKET || cmptr->cmsg_type != SCM_RIGHTS){
            close(conn_fd);
            conn_fd = 0;
            continue;
        }

        ret = *((int *) CMSG_DATA(cmptr));
        close(conn_fd);
        break;
    }

    close(svr_fd);

    if( cycle == MIGRATION_TIMEOUT_CYCLE){
        return  -2;
    }

    return ret;
}

int SocketMigration::send_socket(int sock) {

    int fd = InitUnixSocketClient(env_.MIG_SOCKET_PATH);
    if(fd < 0) {
        return -1;
    }

    struct msghdr msg{};
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_flags = 0;

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    struct iovec iov[1];
    char buf[1];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    struct cmsghdr * cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(cmptr)) = sock;

    int ret = acl_fiber_sendmsg(fd, &msg, 0);
    close(fd);

    return ret;
}

// after start server
int SocketMigration::set_server_pid() {
    set_new_pid();
    return 0;
}

int SocketMigration::get_old_pid() {
    int pid_file = open(env_.SVR_PID_PATH.c_str(), O_RDONLY);
    if(pid_file < 0){
        return -1;
    }

    char buf[15] = {0};
    int n = read(pid_file, buf, 15);
    if(n > 0){
        std::string pid_str(buf);
        pid_t pid = std::stoi(pid_str);
        if(pid > 0){
            old_svr_pid_ = pid;
            close(pid_file);
            return 0;
        }
    }

    close(pid_file);
    return -1;
}

int SocketMigration::set_new_pid() {
    int pid_file = open(env_.SVR_PID_PATH.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if(pid_file < 0){
        return -1;
    }

    char buf[15];
    int n = sprintf(buf, "%d", getpid());
    if(n > 0){
        int wn = write(pid_file, buf, n);
        if(wn > 0){

            close(pid_file);
            return 0;
        }
    }

    close(pid_file);
    return -1;
}
}