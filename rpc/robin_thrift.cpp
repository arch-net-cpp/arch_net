#include "robin_thrift.h"

namespace arch_net { namespace robin {

ThriftConnection::ThriftConnection(RobinThriftServer* server, ISocketStream *stream)
        : server_(server), stream_(stream) {

    nullTransport_ = std::make_shared<TNullTransport>();
    inputTransport_ = std::make_shared<TMemoryBuffer>();
    outputTransport_ = std::make_shared<TMemoryBuffer>();

    factoryInputTransport_ = server_->getInputTransportFactory()->getTransport(inputTransport_);
    factoryOutputTransport_ = server_->getOutputTransportFactory()->getTransport(outputTransport_);

    inputProtocol_ = server_->getInputProtocolFactory()->getProtocol(factoryInputTransport_);
    outputProtocol_ = server_->getOutputProtocolFactory()->getProtocol(factoryOutputTransport_);

    processor_ = server_->getProcessor(inputProtocol_, outputProtocol_, nullTransport_);
}

int ThriftConnection::readN(uint32_t size) {
    inputTransport_->resetBuffer();

    auto buf_begin = inputTransport_->getWritePtr(size);
    while (true) {
        ssize_t n = stream_->recv(buf_begin, size);
        if (n <= 0) {
            return -1;
        }
        inputTransport_->wroteBytes(n);
        size -= n;
        buf_begin = inputTransport_->getWritePtr(size);
        if (size == 0) {
            return 1;
        }
    }
}

int ThriftConnection::process() {
    if (eventHandler_) {
        evHandlerContext_ = eventHandler_->createContext(inputProtocol_, outputProtocol_);
    }

    while (true) {
        Buffer framed_buf(4,0);
        auto n = framed_buf.ReadNFromSocketStream(stream_, 4);
        if (n <= 0) {
            break;
        }
        inputTransport_->resetBuffer();
        uint32_t framed_size = framed_buf.ReadUInt32();

        n = readN(framed_size);
        if (n <= 0) {
            break;
        }
        outputTransport_->resetBuffer();
        outputTransport_->getWritePtr(4);
        outputTransport_->wroteBytes(4);
        try {
            if (eventHandler_) {
                eventHandler_->processContext(evHandlerContext_, nullptr);
            }
            bool process_error = false;
            if (server_->get_workers() != nullptr) {
                acl::wait_group wg;
                wg.add(1);

                server_->get_workers()->addTask([&]() {
                    try {
                        if (!processor_->process(inputProtocol_, outputProtocol_, evHandlerContext_)) {
                            process_error = true;
                        }
                    } catch (...) {
                        process_error = true;
                    }
                    wg.done();
                });
                wg.wait();
                if (process_error) {
                    break;
                }
            }

            if (!processor_->process(inputProtocol_, outputProtocol_, evHandlerContext_)) {
                process_error = true;
            }
            if (process_error) {
                break;
            }

            uint8_t* buf;
            uint32_t size;
            outputTransport_->getBuffer(&buf, &size);
            assert(size >= 4);
            uint32_t frameSize = static_cast<uint32_t>(htonl(size - 4));
            memcpy(buf, &frameSize, 4);
            n = stream_->send(buf, size);
            if(n <= 0) {
                break;
            }
        } catch (const TTransportException& ex) {
            LOG(ERROR) << "ThriftServer TTransportException: " << ex.what();
            break;
        } catch (const std::exception& ex) {
            LOG(ERROR) << "ThriftServer std::exception: " << ex.what();
            break;
        } catch (...) {
            LOG(ERROR) << "ThriftServer unknown exception";
            break;
        }
    }
    close();
    return -1;
}

void ThriftConnection::close() {
    if (eventHandler_) {
        eventHandler_->deleteContext(evHandlerContext_, inputProtocol_, outputProtocol_);
    }

    try {
        inputProtocol_->getTransport()->close();
    } catch (const TTransportException& ttx) {
        LOG(ERROR) <<"TConnectedClient input close failed: " << ttx.what();
    }

    try {
        outputProtocol_->getTransport()->close();
    } catch (const TTransportException& ttx) {
        LOG(ERROR) << "TConnectedClient output close failed: " << ttx.what();
    }

    factoryInputTransport_->close();

    factoryOutputTransport_->close();
}

}}