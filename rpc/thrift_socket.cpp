#include "thrift_socket.h"

namespace apache {
namespace thrift {
namespace transport {

/**
* RTSocket implementation.
*
*/

RTSocket::RTSocket(arch_net::ClientConnection *conn) : connection_(conn) {}

RTSocket::~RTSocket() {
    close();
}

bool RTSocket::isOpen() const {
    return connection_ != nullptr && !socket_closed_;
}

bool RTSocket::peek() {
    if (!isOpen()) {
        return false;
    }
    return true;
}

void RTSocket::open() {
    if (isOpen()) {
        return;
    }
    assert(connection_ != nullptr);
}

void RTSocket::close() {
    connection_->close_connection();
}

uint32_t RTSocket::read(uint8_t* buf, uint32_t len) {
    return connection_->recv_message(buf, len);
}

void RTSocket::write(const uint8_t* buf, uint32_t len) {
    auto ret = connection_->send_message(buf, len);
    if (ret <= 0) {
        socket_closed_ = true;
        throw TTransportException(TTransportException::CLIENT_DISCONNECT, "client connection lost");
    }
}

}}} // apache::thrift::transport
