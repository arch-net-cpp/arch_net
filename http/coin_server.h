#pragma once
#include "../socket_stream.h"
#include "../ssl_socket_stream.h"
#include "../buffer.h"
#include "../application_server.hpp"
#include "../utils/object_pool.hpp"
#include "request.h"
#include "response.h"
#include "restful.h"
#include "websocket.h"


namespace arch_net { namespace coin {

class CoinServer : public ApplicationServer,
                   public RestFul,
                   public CoinWebsocket {

public:
    CoinServer() : ApplicationServer(), RestFul(), CoinWebsocket(){}

private:
    virtual int handle_connection(ISocketStream* stream) {
        LOG(INFO) << "handle new connection " << stream;
        HttpRequest request;
        HttpResponse response;
        while (true) {
            request.reset();
            int ret = request.recv_and_parse(stream);
            if (ret != OK) {
                break;
            }

            response.reset();
            response.keep_alive(request.keep_alive());
            // handle websocket
            if (request.upgrade_websocket()) {
                handle_websocket(request, stream);
                break;
            }

            handle(request, response);

            ret = response.send(stream);
            if (ret <= 0) {
                break;
            }
            if (!response.keep_alive()) {
                break;
            }
        }
        LOG(INFO) << "connection closed " << stream;
        return 0;
    }
};


CoinServer* new_coin_server() {
    return new CoinServer();
}

}}