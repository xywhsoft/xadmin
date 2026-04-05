#ifndef INLINE_ZSTD_H
#define INLINE_ZSTD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZSTDLIB_API
#define ZSTDLIB_API
#endif

#define ZSTD_CLEVEL_DEFAULT 3
#define ZSTD_CONTENTSIZE_UNKNOWN (0ULL - 1)
#define ZSTD_CONTENTSIZE_ERROR (0ULL - 2)

typedef enum {
	ZSTD_error_no_error = 0,
	ZSTD_error_GENERIC = 1,
	ZSTD_error_prefix_unknown = 10,
	ZSTD_error_version_unsupported = 12,
	ZSTD_error_frameParameter_unsupported = 14,
	ZSTD_error_frameParameter_windowTooLarge = 16,
	ZSTD_error_corruption_detected = 20,
	ZSTD_error_checksum_wrong = 22,
	ZSTD_error_literals_headerWrong = 24,
	ZSTD_error_dictionary_corrupted = 30,
	ZSTD_error_dictionary_wrong = 32,
	ZSTD_error_dictionaryCreation_failed = 34,
	ZSTD_error_parameter_unsupported = 40,
	ZSTD_error_parameter_combination_unsupported = 41,
	ZSTD_error_parameter_outOfBound = 42,
	ZSTD_error_tableLog_tooLarge = 44,
	ZSTD_error_maxSymbolValue_tooLarge = 46,
	ZSTD_error_maxSymbolValue_tooSmall = 48,
	ZSTD_error_cannotProduce_uncompressedBlock = 49,
	ZSTD_error_stabilityCondition_notRespected = 50,
	ZSTD_error_stage_wrong = 60,
	ZSTD_error_init_missing = 62,
	ZSTD_error_memory_allocation = 64,
	ZSTD_error_workSpace_tooSmall = 66,
	ZSTD_error_dstSize_tooSmall = 70,
	ZSTD_error_srcSize_wrong = 72,
	ZSTD_error_dstBuffer_null = 74,
	ZSTD_error_noForwardProgress_destFull = 80,
	ZSTD_error_noForwardProgress_inputEmpty = 82,
	ZSTD_error_frameIndex_tooLarge = 100,
	ZSTD_error_seekableIO = 102,
	ZSTD_error_dstBuffer_wrong = 104,
	ZSTD_error_srcBuffer_wrong = 105,
	ZSTD_error_sequenceProducer_failed = 106,
	ZSTD_error_externalSequences_invalid = 107,
	ZSTD_error_maxCode = 120
} ZSTD_ErrorCode;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_CCtx ZSTD_CStream;
typedef ZSTD_DCtx ZSTD_DStream;

typedef struct ZSTD_inBuffer_s {
	const void* src;
	size_t size;
	size_t pos;
} ZSTD_inBuffer;

typedef struct ZSTD_outBuffer_s {
	void* dst;
	size_t size;
	size_t pos;
} ZSTD_outBuffer;

ZSTDLIB_API unsigned ZSTD_versionNumber(void);
ZSTDLIB_API const char* ZSTD_versionString(void);
ZSTDLIB_API size_t ZSTD_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel);
ZSTDLIB_API size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t compressedSize);
ZSTDLIB_API unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize);
ZSTDLIB_API size_t ZSTD_findFrameCompressedSize(const void* src, size_t srcSize);
ZSTDLIB_API size_t ZSTD_compressBound(size_t srcSize);
ZSTDLIB_API unsigned ZSTD_isError(size_t result);
ZSTDLIB_API ZSTD_ErrorCode ZSTD_getErrorCode(size_t result);
ZSTDLIB_API const char* ZSTD_getErrorName(size_t result);
ZSTDLIB_API int ZSTD_minCLevel(void);
ZSTDLIB_API int ZSTD_maxCLevel(void);
ZSTDLIB_API int ZSTD_defaultCLevel(void);

ZSTDLIB_API ZSTD_CCtx* ZSTD_createCCtx(void);
ZSTDLIB_API size_t ZSTD_freeCCtx(ZSTD_CCtx* cctx);
ZSTDLIB_API size_t ZSTD_compressCCtx(ZSTD_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel);
ZSTDLIB_API ZSTD_DCtx* ZSTD_createDCtx(void);
ZSTDLIB_API size_t ZSTD_freeDCtx(ZSTD_DCtx* dctx);
ZSTDLIB_API size_t ZSTD_decompressDCtx(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

ZSTDLIB_API ZSTD_CStream* ZSTD_createCStream(void);
ZSTDLIB_API size_t ZSTD_freeCStream(ZSTD_CStream* zcs);
ZSTDLIB_API size_t ZSTD_initCStream(ZSTD_CStream* zcs, int compressionLevel);
ZSTDLIB_API size_t ZSTD_compressStream(ZSTD_CStream* zcs, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
ZSTDLIB_API size_t ZSTD_flushStream(ZSTD_CStream* zcs, ZSTD_outBuffer* output);
ZSTDLIB_API size_t ZSTD_endStream(ZSTD_CStream* zcs, ZSTD_outBuffer* output);
ZSTDLIB_API size_t ZSTD_CStreamInSize(void);
ZSTDLIB_API size_t ZSTD_CStreamOutSize(void);

ZSTDLIB_API ZSTD_DStream* ZSTD_createDStream(void);
ZSTDLIB_API size_t ZSTD_freeDStream(ZSTD_DStream* zds);
ZSTDLIB_API size_t ZSTD_initDStream(ZSTD_DStream* zds);
ZSTDLIB_API size_t ZSTD_decompressStream(ZSTD_DStream* zds, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
ZSTDLIB_API size_t ZSTD_DStreamInSize(void);
ZSTDLIB_API size_t ZSTD_DStreamOutSize(void);

#ifdef __cplusplus
}
#endif

#endif
