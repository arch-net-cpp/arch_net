#include "echo.pb.h"

#include <gtest/gtest.h>
#include "../rpc/pbrpc_codec.h"

constexpr std::size_t constexpr_strlen(const char* s) {
    return (s && s[0]) ? (constexpr_strlen(&s[1]) + 1) : 0;
}

TEST(TEST_PBRPC, Test_Codec)
{
    example::EchoService_Stub stub(nullptr);

    auto method = stub.descriptor()->method(0);
    arch_net::robin::PBCodec codec;

    ::example::EchoRequest request;
    std::string msg = "11111111111111111111111111111111";
    request.set_message(msg);
    // encode decode request
    {
        arch_net::Buffer meta_buffer;
        arch_net::Buffer data_buffer;
        arch_net::Buffer compress_buffer;
        codec.EncodeRPCRequest(method, CompressType::ZSTD, 0, &request,
                               &meta_buffer, &data_buffer, &compress_buffer);

        auto header_size = meta_buffer.ReadUInt32();
        arch_net::robin::PBRpcReqMeta req_meta;
        codec.DecodeRequestMeta(&meta_buffer, req_meta);
        ::example::EchoRequest recv_msg;
        arch_net::Buffer buffer;
        codec.DecodeRequestData(req_meta, &recv_msg, &compress_buffer, &buffer);
        EXPECT_EQ(msg, recv_msg.message());
    }


    example::EchoResponse response;
    response.set_message(msg);
    {
        arch_net::Buffer meta_buffer;
        arch_net::Buffer data_buffer;
        arch_net::Buffer compress_buffer;
        codec.EncodeRPCResponse(0, "", ZSTD, &response, &meta_buffer, &data_buffer, &compress_buffer);

        auto header_size = meta_buffer.ReadUInt32();
        arch_net::robin::PBRpcRespMeta resp_meta;
        codec.DecodeResponseMeta(&meta_buffer, resp_meta);
        example::EchoResponse recv_response;
        arch_net::Buffer buffer;

        codec.DecodeResponseData(resp_meta, &recv_response,&compress_buffer, &buffer);
        EXPECT_EQ(msg, recv_response.message());
    }


    auto str = "145164184fhgehfghegfhegefheiufheuyfgee";

    std::cout << constexpr_strlen(str) << std::endl;
}

enum MasterElectionState{
    ELT_READY,
    ELT_ZONE_NODE,
    ELT_NORMAL_NODE,
    ELT_FLOWER,
    ELT_CANDIDATE,
    ELT_LEADER,
    ELT_FOREVER,
    ELT_DONE
};

class event {

};

class MessageHeader{

};

class DataPacket : public MessageHeader {
public:
    uint32_t messageId();
    uint32_t source();
    uint32_t destination();
    uint8_t dataType();
    const std::string& rawData();
private:
    uint32_t msg_id;
    uint32_t source_addr;
    uint32_t destination_addr;
    uint8_t data_type;
    std::string header;
    std::string meta_data;
    std::string data;
};

class NeighborInfo {

};
class MasterInfo{};

class DataTransmission {
public:
    uint8_t send(DataPacket* packet);
    uint8_t cancel(uint32_t msg_id);
    void sendDone(DataPacket* packet, uint8_t err);
    void* getMessagePayload(DataPacket* packet);

    DataPacket* receive(DataPacket* data, uint8_t len);
};


class DataForward : DataTransmission {
public:
    uint8_t forwardToNext(DataPacket* packet);
    uint8_t notifySender(DataPacket* packet);
    uint8_t removeDuplicateMessage();
    DataPacket* receiveUpstreamData();

private:
    struct Link {
        uint32_t addr;
        uint32_t acked_num;
        uint32_t noAcked_num;
        bool dead_link;
        uint16_t quality();
    };
    std::map<uint32_t, Link*> link_quality;

    struct router {
        std::list<NeighborInfo*> neighbors;
        MasterInfo* master;
        std::list<MasterInfo*> backup;
        uint32_t getNextAddr();
    };
    router router;

    std::list<DataPacket*> data_queue;
};