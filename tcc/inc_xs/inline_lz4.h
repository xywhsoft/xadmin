#ifndef INLINE_LZ4_H
#define INLINE_LZ4_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LZ4LIB_API
#define LZ4LIB_API
#endif

#define LZ4HC_CLEVEL_MIN 2
#define LZ4HC_CLEVEL_DEFAULT 9
#define LZ4HC_CLEVEL_OPT_MIN 10
#define LZ4HC_CLEVEL_MAX 12

typedef union LZ4_stream_u LZ4_stream_t;
typedef union LZ4_streamHC_u LZ4_streamHC_t;
typedef union LZ4_streamDecode_u LZ4_streamDecode_t;

LZ4LIB_API int LZ4_versionNumber(void);
LZ4LIB_API const char* LZ4_versionString(void);
LZ4LIB_API int LZ4_compressBound(int inputSize);
LZ4LIB_API int LZ4_compress_default(const char* src, char* dst, int srcSize, int dstCapacity);
LZ4LIB_API int LZ4_decompress_safe(const char* src, char* dst, int compressedSize, int dstCapacity);
LZ4LIB_API int LZ4_compress_fast(const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);
LZ4LIB_API int LZ4_sizeofState(void);
LZ4LIB_API int LZ4_compress_fast_extState(void* state, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);

LZ4LIB_API LZ4_stream_t* LZ4_createStream(void);
LZ4LIB_API int LZ4_freeStream(LZ4_stream_t* streamPtr);
LZ4LIB_API void LZ4_resetStream_fast(LZ4_stream_t* streamPtr);
LZ4LIB_API int LZ4_loadDict(LZ4_stream_t* streamPtr, const char* dictionary, int dictSize);
LZ4LIB_API int LZ4_compress_fast_continue(LZ4_stream_t* streamPtr, const char* src, char* dst, int srcSize, int dstCapacity, int acceleration);
LZ4LIB_API int LZ4_saveDict(LZ4_stream_t* streamPtr, char* safeBuffer, int maxDictSize);

LZ4LIB_API LZ4_streamDecode_t* LZ4_createStreamDecode(void);
LZ4LIB_API int LZ4_freeStreamDecode(LZ4_streamDecode_t* streamDecode);
LZ4LIB_API int LZ4_setStreamDecode(LZ4_streamDecode_t* streamDecode, const char* dictionary, int dictSize);
LZ4LIB_API int LZ4_decompress_safe_continue(LZ4_streamDecode_t* streamDecode, const char* src, char* dst, int compressedSize, int dstCapacity);
LZ4LIB_API int LZ4_decompress_safe_usingDict(const char* src, char* dst, int compressedSize, int dstCapacity, const char* dictStart, int dictSize);

LZ4LIB_API int LZ4_compress_HC(const char* src, char* dst, int srcSize, int dstCapacity, int compressionLevel);
LZ4LIB_API int LZ4_sizeofStateHC(void);
LZ4LIB_API int LZ4_compress_HC_extStateHC(void* stateHC, const char* src, char* dst, int srcSize, int maxDstSize, int compressionLevel);

#ifdef __cplusplus
}
#endif

#endif
