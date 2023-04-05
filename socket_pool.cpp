#include "socket_pool.h"

namespace arch_net {

PooledTCPSocketStream::~PooledTCPSocketStream() {
    if (drop || !pool->release(end_point, underlay)) {
        delete underlay;
    }
}

}