#pragma once

#include "zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

static constexpr int GZIP_LEASE_HEADER		= 20;
static constexpr int WINDOW_BITS			= 15;
static constexpr int OPTION_FORMAT_ZLIB		= 0;
static constexpr int OPTION_FORMAT_GZIP		= 16;
static constexpr int OPTION_FORMAT_AUTO		= 32;
/*
* compress serialized msg into buf.
* ret: -1: failed
* 		>0: byte count of compressed data
*/
static int ZlibCompress(const char *msg, size_t msglen,
                        char *buf, size_t buflen) {
    if (!msg)
        return 0;

    z_stream c_stream;

    c_stream.zalloc = (alloc_func) 0;
    c_stream.zfree = (free_func) 0;
    c_stream.opaque = (voidpf) 0;

    if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     WINDOW_BITS | OPTION_FORMAT_ZLIB, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        return -1;
    }

    c_stream.next_in = (Bytef *) msg;
    c_stream.avail_in = msglen;
    c_stream.next_out = (Bytef *) buf;
    c_stream.avail_out = (uInt) buflen;

    while (c_stream.avail_in != 0 && c_stream.total_in < buflen) {
        if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
            return -1;
    }

    if (c_stream.avail_in != 0)
        return c_stream.avail_in;

    for (;;) {
        int err = deflate(&c_stream, Z_FINISH);

        if (err == Z_STREAM_END)
            break;

        if (err != Z_OK)
            return -1;
    }

    if (deflateEnd(&c_stream) != Z_OK)
        return -1;

    return c_stream.total_out;
}


static constexpr unsigned char dummy_head[2] =
        {
                0xB + 0x8 * 0x10,
                (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
        };

/*
 * decompress and parse buf into msg
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */

static int ZlibDecompress(const char *buf, size_t buflen, char *msg, size_t msglen)
{
    int err;
    z_stream d_stream = {0}; /* decompression stream */

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    d_stream.next_in = (Bytef *)buf;
    d_stream.avail_in = 0;
    d_stream.next_out = (Bytef *)msg;
    if (inflateInit2(&d_stream, WINDOW_BITS | OPTION_FORMAT_AUTO) != Z_OK)
        return -1;

    while (d_stream.total_out < msglen && d_stream.total_in < buflen)
    {
        d_stream.avail_in = d_stream.avail_out = (uInt)msglen;
        err = inflate(&d_stream, Z_NO_FLUSH);
        if(err == Z_STREAM_END)
            break;

        if (err != Z_OK)
        {
            if (err != Z_DATA_ERROR)
                return -1;

            d_stream.next_in = (Bytef*) dummy_head;
            d_stream.avail_in = sizeof (dummy_head);
            if (inflate(&d_stream, Z_NO_FLUSH) != Z_OK)
                return -1;
        }
    }

    if (inflateEnd(&d_stream) != Z_OK)
        return -1;

    return d_stream.total_out;
}





#ifdef __cplusplus
}  // extern "C"
#endif