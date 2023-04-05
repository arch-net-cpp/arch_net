#pragma once
#include "iostream"
#include "vector"

#ifdef __cplusplus
extern "C" {
#endif

enum CompressType {
    NO_COMPRESSION = 1,
    LZ4    = 1 << 1,
    LZ4_FAST = 1 << 2,
    SNAPPY = 1 << 3,
    ZSTD   = 1 << 4,
    ZSTD_HIGH = 1 << 5,
};

typedef int64_t (*compress_func)(const char *in, size_t insize, char *out, size_t outsize, size_t, size_t, char*);
typedef char* (*init_func)(size_t insize, size_t, size_t);
typedef void (*deinit_func)(char* workmem);

typedef struct {
    CompressType type;
    int compress_level;
    compress_func compress;
    compress_func decompress;
    init_func init;
    deinit_func deinit;
} compressor_desc_t;

int64_t lz4_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, char*);
int64_t lz4fast_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize,  size_t level, size_t, char*);
int64_t lz4hc_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize,  size_t level, size_t, char*);
int64_t lz4_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);

int64_t snappy_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, char*);
int64_t snappy_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);

int64_t zlib_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);
int64_t zlib_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);

char* zstd_init(size_t insize, size_t level, size_t);
void zstd_deinit(char* workmem);
int64_t zstd_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);
int64_t zstd_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);
char* zstd_LDM_init(size_t insize, size_t level, size_t);
int64_t zstd_LDM_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*);

const compressor_desc_t comp_desc[] = {
        { LZ4,         0, lz4_compress,        lz4_decompress,        nullptr,nullptr},
        { LZ4_FAST,    3, lz4fast_compress,    lz4_decompress,        nullptr,nullptr },
        { LZ4,         1, lz4hc_compress,      lz4_decompress,        nullptr,nullptr },
        { SNAPPY,      0, snappy_compress,     snappy_decompress,     nullptr,nullptr },
        { ZSTD,        1, zstd_compress,       zstd_decompress,       zstd_init,zstd_deinit },
        { ZSTD_HIGH,   5, zstd_compress,       zstd_decompress,       zstd_init,zstd_deinit },
};

#ifdef __cplusplus
}  // extern "C"
#endif

const std::vector<const compressor_desc_t*> compression_funcs = {
        nullptr, nullptr,
        &comp_desc[0],nullptr,
        &comp_desc[1],nullptr,nullptr,nullptr,
        &comp_desc[3],nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
        &comp_desc[4],nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
        &comp_desc[5],nullptr,nullptr,nullptr,
};

namespace arch_net {
class Compression {
public:
    Compression(CompressType type) : type_(type), compressor_(compression_funcs[type]) {
        if (compressor_ && compressor_->init) {
            work_mem_ = compressor_->init(0, 0, 0);
        }
    }

    ~Compression() {
        if (compressor_ && compressor_->deinit && work_mem_) {
            compressor_->deinit(work_mem_);
        }
    }

    int64_t compress(const char* inbuf, size_t insize, char *compbuf, size_t comprsize);

    int64_t decompress(const char *inbuf, size_t insize, char *compbuf, size_t comprsize);
private:
    CompressType type_;
    const compressor_desc_t* compressor_;
    char* work_mem_{nullptr};

};
}
