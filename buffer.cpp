
#include "buffer.h"

namespace arch_net{

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrependSize = 8;
const size_t Buffer::kInitialSize  = 1024;

ssize_t Buffer::ReadFromSocketStream(ISocketStream* stream) {
    size_t writable = WritableBytes();
    if (writable == 0) {
        EnsureWritableBytes(kInitialSize);
        writable = WritableBytes();
    }
    ssize_t n = stream->recv(begin() + write_index_, writable);
    if (n > 0 && n <= writable) {
        write_index_ += n;
    }
    return n;
}

ssize_t Buffer::ReadNFromSocketStream(ISocketStream *stream, uint32_t size) {
    EnsureWritableBytes(size);
    uint32_t readn = size;
    while (true) {
        ssize_t n = stream->recv(begin() + write_index_, size);
        if (n <= 0) {
            return n;
        }
        write_index_ += n;

        size -= n;
        if (size == 0) {
            return readn;
        }
    }
}


}