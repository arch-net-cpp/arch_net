#pragma once

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "../common.h"
#include "../buffer.h"
using google::protobuf::MethodDescriptor;
using google::protobuf::Message;

const uint16_t MAGIC_VALUE = 0x1997;

namespace arch_net { namespace robin {

struct PBRpcReqMeta {
    std::string service_name;
    std::string method_name;
    uint8_t     compress_type;
    uint8_t     streaming_type;
    uint32_t    data_size;
    uint32_t    uncompressed_size;  // optional

    void clear() {
        service_name.clear();
        method_name.clear();
        compress_type = 0;
        streaming_type = 0;
        data_size = 0;
        uncompressed_size = 0;
    }
};

struct PBRpcRespMeta {
    int32_t     status_code;
    std::string error_msg;
    uint8_t     compress_type;
    uint8_t     streaming_type;
    uint32_t    data_size;
    uint32_t    uncompressed_size; // optional
    void clear() {
        status_code = 0;
        error_msg.clear();
        compress_type = 0;
        streaming_type = 0;
        data_size = 0;
        uncompressed_size = 0;
    }
};

class PBCodec {
public:
    static bool encodeHeartBeatRequest(Buffer* buffer) {
        buffer->Reset();
        buffer->AppendUInt32(0);
        return true;
    }

    static bool EncodeRPCRequest(const MethodDescriptor *method, uint8_t compress_type,
                          uint8_t streaming_type, const ::google::protobuf::Message *request_msg,
                          Buffer* meta_buff, Buffer* data_buff, Buffer* compress_buffer=nullptr) {
        auto request_size = request_msg->ByteSizeLong();
        data_buff->Reset();
        data_buff->EnsureWritableBytes(request_size);
        if (!request_msg->SerializeToArray(data_buff->WriteBegin(), data_buff->WritableBytes())) {
            return false;
        }
        data_buff->WriteBytes(request_msg->ByteSizeLong());

        int64_t compress_size = -1;
        if ((CompressType)compress_type != NO_COMPRESSION && compress_buffer) {
            compress_buffer->Reset();
            compress_buffer->EnsureWritableBytes(data_buff->size());
            Compression compressor((CompressType)compress_type);
            compress_size = compressor.compress(data_buff->data(), data_buff->size(),
                                                compress_buffer->WriteBegin(),
                                                compress_buffer->WritableBytes());
            if (compress_size <= 0) {
                return false;
            }
            compress_buffer->WriteBytes(compress_size);
        }

        meta_buff->Reset();
        meta_buff->AppendUInt16(MAGIC_VALUE);
        meta_buff->AppendUInt8(method ? method->service()->name().size() : 0);
        meta_buff->Append(method ? method->service()->name() : "");
        meta_buff->AppendUInt8(method ? method->name().size() : 0);
        meta_buff->Append(method ? method->name() : "");
        meta_buff->AppendUInt8(compress_type);
        meta_buff->AppendUInt8(streaming_type);
        // compressed size
        if ((CompressType)compress_type != NO_COMPRESSION && compress_size > 0 ) {
            meta_buff->AppendUInt32(compress_size);
        }
        // uncompressed size
        meta_buff->AppendUInt32(request_size);
        meta_buff->PrependInt32(meta_buff->size());
        return true;
    }

    bool DecodeRequestMeta(Buffer* buffer, PBRpcReqMeta& req_meta) {
        if (buffer->ReadUInt16() != MAGIC_VALUE) {
            return false;
        }

        req_meta.clear();
        req_meta.service_name = buffer->Next(buffer->ReadUInt8()).ToString();
        req_meta.method_name = buffer->Next(buffer->ReadUInt8()).ToString();

        req_meta.compress_type = buffer->ReadUInt8();
        req_meta.streaming_type = buffer->ReadUInt8();

        req_meta.data_size = buffer->ReadUInt32();
        if (req_meta.compress_type != NO_COMPRESSION) {
            req_meta.uncompressed_size = buffer->ReadUInt32();
        }
        return true;
    }

    bool DecodeRequestData(PBRpcReqMeta& req_meta, ::google::protobuf::Message *recv_msg,
                           Buffer* data_buffer, Buffer* compress_buffer=nullptr) {
        if ((CompressType)req_meta.compress_type == NO_COMPRESSION || !compress_buffer ) {
            return recv_msg->ParseFromArray(data_buffer->data(), data_buffer->size());
        }
        compress_buffer->Reset();
        compress_buffer->EnsureWritableBytes(req_meta.uncompressed_size);
        Compression compressor((CompressType)req_meta.compress_type);
        int64_t size;
        size = compressor.decompress(data_buffer->data(), data_buffer->size(),
                                     compress_buffer->WriteBegin(),
                                     compress_buffer->WritableBytes());
        if (size <= 0) {
            return false;
        }
        compress_buffer->WriteBytes(size);
        assert(size == req_meta.uncompressed_size);
        return recv_msg->ParseFromArray(compress_buffer->data(), compress_buffer->size());
    }

    static bool EncodeRPCErrorResponse(int32_t code, const std::string& err_msg, Buffer* meta_buffer) {
        meta_buffer->Reset();
        meta_buffer->AppendInt32(code);
        meta_buffer->AppendUInt8(err_msg.size());
        meta_buffer->Append(err_msg);
        meta_buffer->AppendUInt8(NO_COMPRESSION);
        meta_buffer->AppendUInt8(0);

        meta_buffer->AppendUInt32(0);
        meta_buffer->PrependInt32(meta_buffer->size());

        return true;
    }
    static bool EncodeRPCResponse(int32_t code, const std::string& err_msg,
            uint8_t compress_type, uint8_t streaming_type, google::protobuf::Message *resp_msg,
            Buffer* meta_buffer, Buffer* data_buffer, Buffer* compress_buffer=nullptr) {

        auto response_size = resp_msg->ByteSizeLong();
        data_buffer->Reset();
        data_buffer->EnsureWritableBytes(response_size);
        if (!resp_msg->SerializeToArray(data_buffer->WriteBegin(), data_buffer->WritableBytes())) {
            return false;
        }
        data_buffer->WriteBytes(resp_msg->ByteSizeLong());
        int64_t compressed_size = -1;
        if ((CompressType)compress_type != NO_COMPRESSION && compress_buffer) {
            compress_buffer->Reset();
            compress_buffer->EnsureWritableBytes(data_buffer->size());
            Compression compressor((CompressType)compress_type);
            compressed_size= compressor.compress(data_buffer->data(), data_buffer->size(),
                                               compress_buffer->WriteBegin(),
                                               compress_buffer->WritableBytes());
            if (compressed_size <= 0) {
                return false;
            }
            compress_buffer->WriteBytes(compressed_size);
        }

        meta_buffer->Reset();
        meta_buffer->AppendInt32(code);
        meta_buffer->AppendUInt8(err_msg.size());
        meta_buffer->Append(err_msg);
        meta_buffer->AppendUInt8(compress_type);
        meta_buffer->AppendUInt8(streaming_type);

        if (compress_type != NO_COMPRESSION && compressed_size > 0) {
            meta_buffer->AppendUInt32(compressed_size);
        }

        meta_buffer->AppendUInt32(response_size);
        meta_buffer->PrependInt32(meta_buffer->size());

        return true;
    }

    static bool DecodeResponseMeta(Buffer* buffer, PBRpcRespMeta& resp_meta) {
        resp_meta.clear();
        resp_meta.status_code = buffer->ReadInt32();
        resp_meta.error_msg = buffer->Next(buffer->ReadUInt8()).ToString();
        resp_meta.compress_type = buffer->ReadUInt8();
        resp_meta.streaming_type = buffer->ReadUInt8();
        resp_meta.data_size = buffer->ReadUInt32();
        if (resp_meta.compress_type != NO_COMPRESSION) {
            resp_meta.uncompressed_size = buffer->ReadUInt32();
        }
        return true;
    }

    static bool DecodeResponseData(PBRpcRespMeta& resp_meta,::google::protobuf::Message *resp_msg, Buffer* buffer, Buffer* compress_buffer=nullptr) {
        if ((CompressType)resp_meta.compress_type == NO_COMPRESSION || !compress_buffer) {
            return resp_msg->ParseFromArray(buffer->data(), buffer->size());
        }

        compress_buffer->Reset();
        compress_buffer->EnsureWritableBytes(resp_meta.uncompressed_size);
        Compression compressor((CompressType)resp_meta.compress_type);
        int64_t size = -1;
        size = compressor.decompress(buffer->data(), buffer->size(),
                                     compress_buffer->WriteBegin(),
                                     compress_buffer->WritableBytes());
        if (size <= 0) {
            return false;
        }
        compress_buffer->WriteBytes(size);
        assert(size == resp_meta.uncompressed_size);
        return resp_msg->ParseFromArray(compress_buffer->data(), compress_buffer->size());
    }

    static bool StreamingEncode(google::protobuf::Message *resp_msg, Buffer* buffer) {
        auto data_buffer = GlobalBufferPool::getInstance().Get();
        defer(GlobalBufferPool::getInstance().Release(data_buffer));

        auto response_size = resp_msg->ByteSizeLong();
        data_buffer->Reset();
        data_buffer->EnsureWritableBytes(response_size);
        if (!resp_msg->SerializeToArray(data_buffer->WriteBegin(), data_buffer->WritableBytes())) {
            return false;
        }
        data_buffer->WriteBytes(resp_msg->ByteSizeLong());

        // set uncompressed buffer size
        buffer->Reset();
        buffer->AppendUInt32(response_size);

        buffer->EnsureWritableBytes(data_buffer->size());
        Compression compressor(CompressType::ZSTD);
        int64_t compressed_size = compressor.compress(data_buffer->data(), data_buffer->size(),
                                             buffer->WriteBegin(),
                                             buffer->WritableBytes());
        if (compressed_size <= 0) {
            return false;
        }
        buffer->WriteBytes(compressed_size);
        return true;
    }

    static bool StreamingDecode(Buffer* buffer, ::google::protobuf::Message *recv_msg) {
        auto compress_buffer = GlobalBufferPool::getInstance().Get();
        defer(GlobalBufferPool::getInstance().Release(compress_buffer));

        auto uncompressed_size = buffer->ReadUInt32();

        compress_buffer->Reset();
        compress_buffer->EnsureWritableBytes(uncompressed_size);
        Compression compressor(CompressType::ZSTD);
        int64_t size;
        size = compressor.decompress(buffer->data(), buffer->size(),
                                     compress_buffer->WriteBegin(),
                                     compress_buffer->WritableBytes());
        if (size <= 0) {
            return false;
        }
        compress_buffer->WriteBytes(size);
        assert(size == uncompressed_size);
        return recv_msg->ParseFromArray(compress_buffer->data(), compress_buffer->size());
    }
};

}}