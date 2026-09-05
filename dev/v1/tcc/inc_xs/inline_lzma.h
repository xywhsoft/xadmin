#ifndef INLINE_LZMA_H
#define INLINE_LZMA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SZ_OK 0
#define SZ_ERROR_DATA 1
#define SZ_ERROR_MEM 2
#define SZ_ERROR_CRC 3
#define SZ_ERROR_UNSUPPORTED 4
#define SZ_ERROR_PARAM 5
#define SZ_ERROR_INPUT_EOF 6
#define SZ_ERROR_OUTPUT_EOF 7
#define SZ_ERROR_READ 8
#define SZ_ERROR_WRITE 9
#define SZ_ERROR_PROGRESS 10
#define SZ_ERROR_FAIL 11
#define SZ_ERROR_THREAD 12

typedef int SRes;
typedef unsigned char Byte;
typedef int32_t Int32;
typedef uint32_t UInt32;
typedef int64_t Int64;
typedef uint64_t UInt64;
typedef size_t SizeT;

typedef struct ISeqInStream ISeqInStream;
typedef const ISeqInStream* ISeqInStreamPtr;
struct ISeqInStream
{
	SRes (*Read)(ISeqInStreamPtr p, void* buf, size_t* size);
};

typedef struct ISeqOutStream ISeqOutStream;
typedef const ISeqOutStream* ISeqOutStreamPtr;
struct ISeqOutStream
{
	size_t (*Write)(ISeqOutStreamPtr p, const void* buf, size_t size);
};

typedef struct ICompressProgress ICompressProgress;
typedef const ICompressProgress* ICompressProgressPtr;
struct ICompressProgress
{
	SRes (*Progress)(ICompressProgressPtr p, UInt64 inSize, UInt64 outSize);
};

typedef struct ISzAlloc ISzAlloc;
typedef const ISzAlloc* ISzAllocPtr;
struct ISzAlloc
{
	void* (*Alloc)(ISzAllocPtr p, size_t size);
	void (*Free)(ISzAllocPtr p, void* address);
};

extern const ISzAlloc g_Alloc;
extern const ISzAlloc g_BigAlloc;
extern const ISzAlloc g_AlignedAlloc;

void* MyAlloc(size_t size);
void MyFree(void* address);
void* MyRealloc(void* address, size_t size);

#define LZMA_PROPS_SIZE 5

typedef struct
{
	Byte lc;
	Byte lp;
	Byte pb;
	Byte _pad_;
	UInt32 dicSize;
} CLzmaProps;

typedef enum
{
	LZMA_FINISH_ANY,
	LZMA_FINISH_END
} ELzmaFinishMode;

typedef enum
{
	LZMA_STATUS_NOT_SPECIFIED,
	LZMA_STATUS_FINISHED_WITH_MARK,
	LZMA_STATUS_NOT_FINISHED,
	LZMA_STATUS_NEEDS_MORE_INPUT,
	LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK
} ELzmaStatus;

SRes LzmaProps_Decode(CLzmaProps* p, const Byte* data, unsigned size);
SRes LzmaDecode(Byte* dest, SizeT* destLen, const Byte* src, SizeT* srcLen, const Byte* propData, unsigned propSize, ELzmaFinishMode finishMode, ELzmaStatus* status, ISzAllocPtr alloc);

typedef struct
{
	int level;
	UInt32 dictSize;
	int lc;
	int lp;
	int pb;
	int algo;
	int fb;
	int btMode;
	int numHashBytes;
	unsigned numHashOutBits;
	UInt32 mc;
	unsigned writeEndMark;
	int numThreads;
	Int32 affinityGroup;
	UInt64 reduceSize;
	UInt64 affinity;
	UInt64 affinityInGroup;
} CLzmaEncProps;

void LzmaEncProps_Init(CLzmaEncProps* p);
SRes LzmaEncode(Byte* dest, SizeT* destLen, const Byte* src, SizeT srcLen, const CLzmaEncProps* props, Byte* propsEncoded, SizeT* propsSize, int writeEndMark, ICompressProgressPtr progress, ISzAllocPtr alloc, ISzAllocPtr allocBig);

typedef struct CLzma2Enc CLzma2Enc;
typedef CLzma2Enc* CLzma2EncHandle;

typedef struct
{
	CLzmaEncProps lzmaProps;
	UInt64 blockSize;
	int numBlockThreads_Reduced;
	int numBlockThreads_Max;
	int numTotalThreads;
	unsigned numThreadGroups;
} CLzma2EncProps;

void Lzma2EncProps_Init(CLzma2EncProps* p);
CLzma2EncHandle Lzma2Enc_Create(ISzAllocPtr alloc, ISzAllocPtr allocBig);
void Lzma2Enc_Destroy(CLzma2EncHandle p);
SRes Lzma2Enc_SetProps(CLzma2EncHandle p, const CLzma2EncProps* props);
void Lzma2Enc_SetDataSize(CLzma2EncHandle p, UInt64 expectedDataSiize);
Byte Lzma2Enc_WriteProperties(CLzma2EncHandle p);
SRes Lzma2Enc_Encode2(CLzma2EncHandle p, ISeqOutStreamPtr outStream, Byte* outBuf, size_t* outBufSize, ISeqInStreamPtr inStream, const Byte* inData, size_t inDataSize, ICompressProgressPtr progress);

SRes Lzma2Decode(Byte* dest, SizeT* destLen, const Byte* src, SizeT* srcLen, Byte prop, ELzmaFinishMode finishMode, ELzmaStatus* status, ISzAllocPtr alloc);

#ifdef __cplusplus
}
#endif

#endif
