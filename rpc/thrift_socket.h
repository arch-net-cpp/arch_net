#pragma once
#include "../socket_stream.h"
#include "../ssl_socket_stream.h"
#include "../application_client.h"
#include "../utils/future/future.h"

#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>
#include <thrift/transport/TBufferTransports.h>

using namespace apache::thrift::transport;

namespace apache {
namespace thrift {
namespace transport {

class RTSocket : public TVirtualTransport<RTSocket> {
public:
    RTSocket(arch_net::ClientConnection* conn);

    /**
     * Destroyes the socket object, closing it if necessary.
     */
    ~RTSocket() override;

    /**
     * Whether the socket is alive.
     *
     * @return Is the socket alive?
     */
    bool isOpen() const override;

    /**
     * Checks whether there is more data available in the socket to read.
     *
     * This call blocks until at least one byte is available or the socket is closed.
     */
    bool peek() override;

    /**
     * Creates and opens the UNIX socket.
     *
     * @throws TTransportException If the socket could not connect
     */
    void open() override;

    /**
     * Shuts down communications on the socket.
     */
    void close() override;

    /**
     * Reads from the underlying socket.
     * \returns the number of bytes read or 0 indicates EOF
     * \throws TTransportException of types:
     *           INTERRUPTED means the socket was interrupted
     *                       out of a blocking call
     *           NOT_OPEN means the socket has been closed
     *           TIMED_OUT means the receive timeout expired
     *           UNKNOWN means something unexpected happened
     */
    virtual uint32_t read(uint8_t* buf, uint32_t len);

    /**
     * Writes to the underlying socket.  Loops until done or fail.
     */
    virtual void write(const uint8_t* buf, uint32_t len);

private:
    std::unique_ptr<arch_net::ClientConnection> connection_;
    bool socket_closed_{false};
};
}
}
}
