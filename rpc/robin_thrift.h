#pragma once
#include "../application_server.hpp"
#include "../buffer.h"

#include <thrift/server/TServer.h>
#include <thrift/protocol/TProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

using apache::thrift::TProcessor;
using apache::thrift::TProcessorFactory;
using apache::thrift::protocol::TProtocolFactory;
using apache::thrift::server::TServer;
using apache::thrift::transport::TTransportFactory;

using apache::thrift::protocol::TProtocol;
using apache::thrift::transport::TMemoryBuffer;
using apache::thrift::transport::TNullTransport;
using apache::thrift::transport::TTransport;
using apache::thrift::transport::TTransportException;
using apache::thrift::server::TServerEventHandler;

namespace arch_net { namespace robin {

class RobinThriftServer;

class ThriftConnection : public TNonCopyable {
public:
    ThriftConnection(RobinThriftServer* server, ISocketStream* stream);

    int process();

private:
    int readN(uint32_t size);
    void close();

private:
    RobinThriftServer* server_;
    ISocketStream* stream_;

    std::shared_ptr<TTransport> nullTransport_;
    std::shared_ptr<TMemoryBuffer> inputTransport_;
    std::shared_ptr<TMemoryBuffer> outputTransport_;
    std::shared_ptr<TTransport> factoryInputTransport_;
    std::shared_ptr<TTransport> factoryOutputTransport_;
    std::shared_ptr<TProtocol> inputProtocol_;
    std::shared_ptr<TProtocol> outputProtocol_;
    std::shared_ptr<TProcessor> processor_;
    std::shared_ptr<apache::thrift::server::TServerEventHandler> eventHandler_;
    //Context acquired from the eventHandler_ if one exists.
    void* evHandlerContext_;
};


class RobinThriftServer : public TServer,
                          public ApplicationServer {
public:

    RobinThriftServer(const std::shared_ptr<TProcessor>& processor, bool use_ssl = false)
    : TServer(processor), ApplicationServer(), worker_pool_(){}

    RobinThriftServer(const std::shared_ptr<TProcessor>& processor,
            const std::shared_ptr<TProtocolFactory>& protocolFactory, bool use_ssl = false)
    : TServer(processor), ApplicationServer(), worker_pool_() {
        setInputProtocolFactory(protocolFactory);
        setOutputProtocolFactory(protocolFactory);
    }

    std::shared_ptr<TProcessor> getProcessor(
            std::shared_ptr<TProtocol>& inputProtocol,
            std::shared_ptr<TProtocol>& outputProtocol,
            std::shared_ptr<TTransport>& transport) {

        return TServer::getProcessor(inputProtocol, outputProtocol, transport);
    }

    WorkerPool* get_workers() {
        return worker_pool_.get();
    }

private:
    int handle_connection(ISocketStream* stream) override {
        LOG(INFO) << "handle new connection " << stream;

        ThriftConnection connection(this, stream);
        connection.process();

        LOG(INFO) << "connection closed " << stream;
        return 0;
    }
    void serve() override {}
private:
    std::unique_ptr<WorkerPool> worker_pool_;
};


}}