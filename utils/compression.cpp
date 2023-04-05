
#include "compression.h"

#include <lz4.h>
#include <lz4hc.h>
#include <snappy.h>
#define ZSTD_STATIC_LINKING_ONLY

#include <zstd.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t lz4_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, char*)
{
    return LZ4_compress_default(inbuf, outbuf, insize, outsize);
}

int64_t lz4fast_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, char*)
{
    return LZ4_compress_fast(inbuf, outbuf, insize, outsize, level);
}

int64_t lz4hc_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t, char*)
{
    return LZ4_compress_HC(inbuf, outbuf, insize, outsize, level);
}

int64_t lz4_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*)
{
    return LZ4_decompress_safe(inbuf, outbuf, insize, outsize);
}


int64_t snappy_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*)
{
    snappy::RawCompress(inbuf, insize, outbuf, &outsize);
    return outsize;
}

int64_t snappy_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char*)
{
    snappy::RawUncompress(inbuf, insize, outbuf);
    return outsize;
}


typedef struct {
    ZSTD_CCtx* cctx;
    ZSTD_DCtx* dctx;
    ZSTD_CDict* cdict;
    ZSTD_parameters zparams;
    ZSTD_customMem cmem;
} zstd_params_s;

char* zstd_init(size_t insize, size_t level, size_t windowLog)
{
    zstd_params_s* zstd_params = (zstd_params_s*) malloc(sizeof(zstd_params_s));
    if (!zstd_params) return NULL;
    zstd_params->cctx = ZSTD_createCCtx();
    zstd_params->dctx = ZSTD_createDCtx();
#if 1
    zstd_params->cdict = NULL;
#else
    zstd_params->zparams = ZSTD_getParams(level, insize, 0);
    zstd_params->cmem = { NULL, NULL, NULL };
    if (windowLog && zstd_params->zparams.cParams.windowLog > windowLog) {
        zstd_params->zparams.cParams.windowLog = windowLog;
        zstd_params->zparams.cParams.chainLog = windowLog + ((zstd_params->zparams.cParams.strategy == ZSTD_btlazy2) | (zstd_params->zparams.cParams.strategy == ZSTD_btopt) | (zstd_params->zparams.cParams.strategy == ZSTD_btopt2));
    }
    zstd_params->cdict = ZSTD_createCDict_advanced(NULL, 0, zstd_params->zparams, zstd_params->cmem);
#endif

    return (char*) zstd_params;
}

void zstd_deinit(char* workmem)
{
    zstd_params_s* zstd_params = (zstd_params_s*) workmem;
    if (!zstd_params) return;
    if (zstd_params->cctx) ZSTD_freeCCtx(zstd_params->cctx);
    if (zstd_params->dctx) ZSTD_freeDCtx(zstd_params->dctx);
    if (zstd_params->cdict) ZSTD_freeCDict(zstd_params->cdict);
    free(workmem);
}

int64_t zstd_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t windowLog, char* workmem)
{
    size_t res;

    zstd_params_s* zstd_params = (zstd_params_s*) workmem;
    if (!zstd_params || !zstd_params->cctx) return 0;

#if 1
    zstd_params->zparams = ZSTD_getParams(level, insize, 0);
    ZSTD_CCtx_setParameter(zstd_params->cctx, ZSTD_c_compressionLevel, level);
    zstd_params->zparams.fParams.contentSizeFlag = 1;

    if (windowLog && zstd_params->zparams.cParams.windowLog > windowLog) {
        zstd_params->zparams.cParams.windowLog = windowLog;
        zstd_params->zparams.cParams.chainLog = windowLog + ((zstd_params->zparams.cParams.strategy == ZSTD_btlazy2) || (zstd_params->zparams.cParams.strategy == ZSTD_btopt) || (zstd_params->zparams.cParams.strategy == ZSTD_btultra));
    }
    res = ZSTD_compress_advanced(zstd_params->cctx, outbuf, outsize, inbuf, insize, nullptr, 0, zstd_params->zparams);
//    res = ZSTD_compressCCtx(zstd_params->cctx, outbuf, outsize, inbuf, insize, level);
#else
    if (!zstd_params->cdict) return 0;
    res = ZSTD_compress_usingCDict(zstd_params->cctx, outbuf, outsize, inbuf, insize, zstd_params->cdict);
#endif
    if (ZSTD_isError(res)) return res;

    return res;
}

int64_t zstd_decompress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t, size_t, char* workmem)
{
    zstd_params_s* zstd_params = (zstd_params_s*) workmem;
    if (!zstd_params || !zstd_params->dctx) return 0;

    return ZSTD_decompressDCtx(zstd_params->dctx, outbuf, outsize, inbuf, insize);
}

char* zstd_LDM_init(size_t insize, size_t level, size_t windowLog)
{
    zstd_params_s* zstd_params = (zstd_params_s*) zstd_init(insize, level, windowLog);
    if (!zstd_params) return NULL;
    ZSTD_CCtx_setParameter(zstd_params->cctx, ZSTD_c_enableLongDistanceMatching, 1);
    return (char*) zstd_params;
}

int64_t zstd_LDM_compress(const char *inbuf, size_t insize, char *outbuf, size_t outsize, size_t level, size_t windowLog, char* workmem) {
    zstd_params_s* zstd_params = (zstd_params_s*) workmem;
    if (!zstd_params || !zstd_params->cctx) return 0;
    ZSTD_CCtx_setParameter(zstd_params->cctx, ZSTD_c_enableLongDistanceMatching, 1);
    return zstd_compress(inbuf, insize, outbuf, outsize, level, windowLog, (char*) zstd_params);
}

#ifdef __cplusplus
}  // extern "C"
#endif


namespace arch_net {
int64_t Compression::compress(const char *inbuf, size_t insize, char *compbuf, size_t comprsize) {
    if (!compressor_) {
        return -1;
    }
    return compressor_->compress(inbuf, insize, (char*)compbuf, comprsize,
                                 compressor_->compress_level, 0, work_mem_);
}

int64_t Compression::decompress(const char *inbuf, size_t insize, char *compbuf, size_t comprsize){
    if (!compressor_) {
        return -1;
    }
    return compressor_->decompress(inbuf, insize, (char*)compbuf, comprsize, 0, 0, work_mem_);
}

}