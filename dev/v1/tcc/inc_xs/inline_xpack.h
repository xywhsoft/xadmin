/*
	xPack 公开头文件

	负责声明 xPack 的公开常量、数据结构与 API，
	同时在 XPACK_BUILD_CORE 模式下暴露内部实现共享的核心声明。
*/

#ifndef XPACK_H
#define XPACK_H

#include <stdint.h>


// 统一时间类型，和 xrt 的 xtime 保持兼容
#ifndef XPK_XTIME_DEFINED
#define XPK_XTIME_DEFINED
typedef int64_t xtime;
#endif


#ifdef __cplusplus
extern "C" {
#endif


// 动态库导出宏
#if defined(_WIN32) || defined(_WIN64)
	#if defined(XPK_BUILD_DLL)
		#define XPKAPI __declspec(dllexport)
	#elif defined(XPK_USE_DLL)
		#define XPKAPI __declspec(dllimport)
	#else
		#define XPKAPI
	#endif
#else
	#define XPKAPI
#endif


// 文件头与布局常量
#define XPK_FILE_HEAD        0x116B7078u
#define XPK_HEAD_SIZE        64u
#define XPK_VOLUME_MIN       0x00010000u
#define XPK_VOLUME_MAX       0xFFFFFFFFu
#define XPK_PATH_BYTES       260u
#define XPK_PATH_ENTRY_SIZE  320u


// 文件类型标记
#define XPK_TYPE_CORE        0u
#define XPK_TYPE_INDEX       1u
#define XPK_TYPE_LINUX       2u
#define XPK_TYPE_WIN32       3u


// 压缩算法标记
#define XPK_ALG_STORE        0u
#define XPK_ALG_LZ4          1u
#define XPK_ALG_LZ4HC        2u
#define XPK_ALG_ZSTD         3u
#define XPK_ALG_LZMA2        4u


// 条目标记掩码
#define XPK_FLAG_COMP_MASK     0x0000000Fu
#define XPK_FLAG_TYPE_MASK     0x000000F0u
#define XPK_FLAG_DELETED_MASK  0x00000100u


// 包对象句柄
typedef struct xpkStruct* xpkObject;


// 包类型
typedef enum xpkPackType {
	XPK_PACK_CORE = 0,
	XPK_PACK_INDEX = 1,
	XPK_PACK_LINUX = 2,
	XPK_PACK_WIN32 = 3
} xpkPackType;


// 写入策略
typedef enum xpkWritePolicy {
	XPK_WRITE_BUFFERED = 0,
	XPK_WRITE_IMMEDIATE = 1
} xpkWritePolicy;


// 错误码
typedef enum xpkErrorCode {
	XPK_OK = 0,
	XPK_ERR_PARAM = -1,
	XPK_ERR_STATE = -2,
	XPK_ERR_MEMORY = -3,
	XPK_ERR_IO = -4,
	XPK_ERR_FORMAT = -5,
	XPK_ERR_HASH = -6,
	XPK_ERR_NOT_FOUND = -7,
	XPK_ERR_EXISTS = -8,
	XPK_ERR_SOLID_DATA_WRITE = -9,
	XPK_ERR_READONLY = -10,
	XPK_ERR_UNSUPPORTED = -11
} xpkErrorCode;


// 打开选项
typedef struct xpkOpenOptions {
	uint8_t readonly;         // 是否按只读模式打开
	uint8_t createIfMissing;  // 目标不存在时是否创建空包
	uint8_t bufferedDefault;  // 默认写入策略是否使用缓冲模式
	uint8_t reserved0;        // 保留字段
} xpkOpenOptions;


// 写入选项
typedef struct xpkWriteOptions {
	uint8_t compLevel;    // 压缩级别
	uint8_t writePolicy;  // 写入策略
	uint8_t fileType;     // 条目类型
	uint8_t reserved0;    // 保留字段
} xpkWriteOptions;


// 重构选项
typedef struct xpkBuildOptions {
	const char* tempPath;      // 临时输出路径
	uint8_t replaceOriginal;   // 是否替换原始包
	uint8_t reserved0[7];      // 保留字段
} xpkBuildOptions;


// 包统计信息
typedef struct xpkStat {
	uint32_t fileCount;       // 可见条目数量
	uint64_t liveDataBytes;   // 有效数据大小
	uint64_t holeBytes;       // 空洞大小
	uint64_t metaBytes;       // 元数据大小
	uint64_t entryTableBytes; // 条目表大小
} xpkStat;


// 对齐到磁盘布局的包头结构
#pragma pack(push, 1)
typedef struct {
	uint32_t fileHead;       // 包魔数
	uint32_t fileCount;      // 条目数量

	uint32_t packType    : 2;   // 包类型
	uint32_t defComp     : 4;   // 默认数据压缩级别
	uint32_t metaComp    : 4;   // 元数据压缩级别
	uint32_t infoComp    : 4;   // 条目表压缩级别
	uint32_t infoExtSize : 18;  // Core 扩展信息大小

	uint32_t solidMode   : 1;   // 是否启用 solid 布局
	uint32_t volumeMode  : 1;   // 是否启用分卷布局
	uint32_t reserved1   : 30;  // 预留位

	uint64_t dataOffset;     // 数据区起始逻辑偏移
	uint32_t volumeSize;     // 分卷大小

	uint32_t metaRawSize;    // 元数据原始大小
	uint32_t metaCompSize;   // 元数据压缩后大小
	uint32_t metaHash;       // 元数据哈希

	uint32_t infoCompSize;   // 条目表压缩后大小
	uint32_t infoHash;       // 条目表哈希

	xtime createTime;        // 创建时间
	xtime changeTime;        // 最后修改时间
} xpkHead;


// Core 模式下的基础条目信息
typedef struct {
	uint32_t flag;          // 条目标记
	uint32_t fileHash;      // 文件内容哈希
	uint64_t dataOffset;    // 数据偏移
	uint64_t dataSize;      // 存储大小
	uint64_t fileSize;      // 原始大小
} xpkFileInfo;


// Index 模式下的条目信息
typedef struct {
	uint32_t flag;          // 条目标记
	uint32_t fileHash;      // 文件内容哈希
	uint64_t dataOffset;    // 数据偏移
	uint64_t dataSize;      // 存储大小
	uint64_t fileSize;      // 原始大小
	int64_t fileIndex;      // 逻辑索引号
} xpkFileInfoIndex;


// Path 模式下的条目信息
typedef struct {
	uint32_t flag;                     // 条目标记
	uint32_t fileHash;                 // 文件内容哈希
	uint64_t dataOffset;               // 数据偏移
	uint64_t dataSize;                 // 存储大小
	uint64_t fileSize;                 // 原始大小
	char pathBytes[XPK_PATH_BYTES];    // 包内路径
	uint32_t platformAttr;             // 平台属性
	uint64_t createTime;               // 创建时间
	uint64_t modifyTime;               // 修改时间
	uint64_t accessTime;               // 访问时间
} xpkFileInfoPath;
#pragma pack(pop)


// 布局静态断言
typedef char xpkStaticAssertHeadSize[(sizeof(xpkHead) == XPK_HEAD_SIZE) ? 1 : -1];
typedef char xpkStaticAssertFileInfoSize[(sizeof(xpkFileInfo) == 32u) ? 1 : -1];
typedef char xpkStaticAssertFileInfoIndexSize[(sizeof(xpkFileInfoIndex) == 40u) ? 1 : -1];
typedef char xpkStaticAssertFileInfoPathSize[(sizeof(xpkFileInfoPath) == XPK_PATH_ENTRY_SIZE) ? 1 : -1];


// 遍历回调
typedef int (*xpkEachProc)(xpkObject xpk, uint32_t pos, const void* info, void* userData);


// 包对象管理
// 打开包并创建运行时对象
XPKAPI xpkObject xpkOpen(const char* packagePath, const xpkOpenOptions* options);

// 关闭包对象并释放运行时资源
XPKAPI int xpkClose(xpkObject xpk);

// 将当前脏状态保存回包文件
XPKAPI int xpkSave(xpkObject xpk);

// 按目标布局重构包文件
XPKAPI int xpkBuild(xpkObject xpk, const xpkBuildOptions* options);


// 包级配置
// 获取当前包类型
XPKAPI int xpkGetPackType(xpkObject xpk, xpkPackType* outType);

// 设置当前包类型
XPKAPI int xpkSetPackType(xpkObject xpk, xpkPackType type);

// 获取默认数据压缩级别
XPKAPI int xpkGetDefaultComp(xpkObject xpk, uint8_t* outLevel);

// 设置默认数据压缩级别
XPKAPI int xpkSetDefaultComp(xpkObject xpk, uint8_t level);

// 获取元数据压缩级别
XPKAPI int xpkGetMetaComp(xpkObject xpk, uint8_t* outLevel);

// 设置元数据压缩级别
XPKAPI int xpkSetMetaComp(xpkObject xpk, uint8_t level);

// 获取条目表压缩级别
XPKAPI int xpkGetInfoComp(xpkObject xpk, uint8_t* outLevel);

// 设置条目表压缩级别
XPKAPI int xpkSetInfoComp(xpkObject xpk, uint8_t level);

// 获取条目信息扩展大小
XPKAPI int xpkGetInfoExtSize(xpkObject xpk, uint32_t* outSize);

// 设置条目信息扩展大小
XPKAPI int xpkSetInfoExtSize(xpkObject xpk, uint32_t size);

// 获取分卷大小
XPKAPI int xpkGetVolumeSize(xpkObject xpk, uint32_t* outSize);

// 设置分卷大小
XPKAPI int xpkSetVolumeSize(xpkObject xpk, uint32_t size);

// 获取 Solid 模式开关
XPKAPI int xpkGetSolidMode(xpkObject xpk, int* outEnabled);

// 设置 Solid 模式开关
XPKAPI int xpkSetSolidMode(xpkObject xpk, int enabled);


// 包元数据
// 获取包元数据缓冲
XPKAPI void* xpkMetaGet(xpkObject xpk, uint32_t* outSize);

// 设置包元数据缓冲
XPKAPI int xpkMetaSet(xpkObject xpk, const void* data, uint32_t size, uint8_t compLevel);

// 清空包元数据
XPKAPI int xpkMetaClear(xpkObject xpk);


// 按位置访问条目
// 获取可见条目数量
XPKAPI uint32_t xpkCount(xpkObject xpk);

// 获取指定位置的基础条目信息
XPKAPI int xpkGetInfo(xpkObject xpk, uint32_t pos, xpkFileInfo* outInfo);

// 获取指定位置的扩展信息
XPKAPI int xpkGetInfoExt(xpkObject xpk, uint32_t pos, void* outData, uint32_t size);

// 设置指定位置的扩展信息
XPKAPI int xpkSetInfoExt(xpkObject xpk, uint32_t pos, const void* data, uint32_t size);

// 在指定位置添加文件条目
XPKAPI int xpkAddFile(xpkObject xpk, const char* srcPath, const xpkWriteOptions* options, uint32_t* outPos);

// 在指定位置添加内存数据条目
XPKAPI int xpkAddData(xpkObject xpk, const void* data, uint64_t size, const xpkWriteOptions* options, uint32_t* outPos);

// 将指定位置条目导出到文件
XPKAPI int xpkReadToFile(xpkObject xpk, uint32_t pos, const char* dstPath);

// 将指定位置条目读入内存
XPKAPI void* xpkReadToMemory(xpkObject xpk, uint32_t pos, uint64_t* outSize);

// 用文件内容更新指定位置条目
XPKAPI int xpkUpdateFile(xpkObject xpk, uint32_t pos, const char* srcPath, const xpkWriteOptions* options);

// 用内存数据更新指定位置条目
XPKAPI int xpkUpdateData(xpkObject xpk, uint32_t pos, const void* data, uint64_t size, const xpkWriteOptions* options);

// 移除指定位置条目
XPKAPI int xpkRemove(xpkObject xpk, uint32_t pos);

// 修改指定位置条目标记
XPKAPI int xpkSetFlag(xpkObject xpk, uint32_t pos, uint32_t mask, uint32_t value);


// 按索引访问条目
// 通过 fileIndex 查找条目位置
XPKAPI int xpkIndexFind(xpkObject xpk, int64_t fileIndex, uint32_t* outPos);

// 通过 fileIndex 获取条目信息
XPKAPI int xpkIndexGetInfo(xpkObject xpk, int64_t fileIndex, xpkFileInfoIndex* outInfo);

// 通过 fileIndex 添加文件条目
XPKAPI int xpkIndexAddFile(xpkObject xpk, int64_t fileIndex, const char* srcPath, const xpkWriteOptions* options);

// 通过 fileIndex 添加内存数据条目
XPKAPI int xpkIndexAddData(xpkObject xpk, int64_t fileIndex, const void* data, uint64_t size, const xpkWriteOptions* options);

// 通过 fileIndex 导出条目到文件
XPKAPI int xpkIndexReadToFile(xpkObject xpk, int64_t fileIndex, const char* dstPath);

// 通过 fileIndex 读取条目到内存
XPKAPI void* xpkIndexReadToMemory(xpkObject xpk, int64_t fileIndex, uint64_t* outSize);

// 通过 fileIndex 更新文件条目
XPKAPI int xpkIndexUpdateFile(xpkObject xpk, int64_t fileIndex, const char* srcPath, const xpkWriteOptions* options);

// 通过 fileIndex 更新内存数据条目
XPKAPI int xpkIndexUpdateData(xpkObject xpk, int64_t fileIndex, const void* data, uint64_t size, const xpkWriteOptions* options);

// 通过 fileIndex 移除条目
XPKAPI int xpkIndexRemove(xpkObject xpk, int64_t fileIndex);

// 通过 fileIndex 修改条目标记
XPKAPI int xpkIndexSetFlag(xpkObject xpk, int64_t fileIndex, uint32_t mask, uint32_t value);


// 按路径访问条目
// 判断包内路径是否存在
XPKAPI int xpkPathExists(xpkObject xpk, const char* packagePath);

// 获取包内路径对应的条目信息
XPKAPI int xpkPathGetInfo(xpkObject xpk, const char* packagePath, xpkFileInfoPath* outInfo);

// 按包内路径添加文件条目
XPKAPI int xpkPathAddFile(xpkObject xpk, const char* packagePath, const char* srcPath, const xpkWriteOptions* options);

// 按包内路径添加内存数据条目
XPKAPI int xpkPathAddData(xpkObject xpk, const char* packagePath, const void* data, uint64_t size, const xpkWriteOptions* options);

// 按包内路径导出条目到文件
XPKAPI int xpkPathReadToFile(xpkObject xpk, const char* packagePath, const char* dstPath);

// 按包内路径读取条目到内存
XPKAPI void* xpkPathReadToMemory(xpkObject xpk, const char* packagePath, uint64_t* outSize);

// 按包内路径更新文件条目
XPKAPI int xpkPathUpdateFile(xpkObject xpk, const char* packagePath, const char* srcPath, const xpkWriteOptions* options);

// 按包内路径更新内存数据条目
XPKAPI int xpkPathUpdateData(xpkObject xpk, const char* packagePath, const void* data, uint64_t size, const xpkWriteOptions* options);

// 重命名包内路径
XPKAPI int xpkPathRename(xpkObject xpk, const char* oldPath, const char* newPath);

// 按包内路径移除条目
XPKAPI int xpkPathRemove(xpkObject xpk, const char* packagePath);

// 按包内路径设置平台属性
XPKAPI int xpkPathSetAttr(xpkObject xpk, const char* packagePath, uint32_t platformAttr);


// 遍历与筛选
// 遍历全部可见条目
XPKAPI int xpkEach(xpkObject xpk, xpkEachProc proc, void* userData);

// 按模式遍历匹配条目
XPKAPI int xpkEachMatch(xpkObject xpk, const char* pattern, xpkEachProc proc, void* userData);


// 校验、统计与基础工具
// 校验单个条目内容
XPKAPI int xpkVerify(xpkObject xpk, uint32_t pos);

// 校验全部条目内容
XPKAPI int xpkVerifyAll(xpkObject xpk);

// 获取当前包统计信息
XPKAPI int xpkStatGet(xpkObject xpk, xpkStat* outStat);

// 释放 xPack 分配的返回内存
XPKAPI void xpkFree(void* ptr);

// 计算 32 位哈希值
XPKAPI uint32_t xpkHash32(const void* data, uint64_t size);


// 错误状态
// 获取最后错误码
XPKAPI xpkErrorCode xpkLastError(xpkObject xpk);

// 获取最后错误文本
XPKAPI const char* xpkLastErrorMessage(xpkObject xpk);


#ifdef __cplusplus
}
#endif


#if defined(XPACK_BUILD_CORE)

/*
	xPack 内部共享声明

	该部分只在实现编译单元中可见，
	要求在包含前先准备好 xrt 与压缩库头文件。
*/


// 错误文本缓存大小
#define XPK_ERROR_TEXT_CAP 256


// 未使用参数标记
#if defined(__GNUC__) || defined(__clang__)
	#define XPK_UNUSED __attribute__((unused))
#else
	#define XPK_UNUSED
#endif


// 内部布局常量
enum {
	XPK_INTERNAL_ERROR_TEXT_CAP = XPK_ERROR_TEXT_CAP,
	XPK_ENTRY_BASE_SIZE = 32,
	XPK_INFO_EXT_INDEX = 8,
	XPK_INFO_EXT_PATH = 288
};


// 内部前置声明
typedef struct xpkErrorState xpkErrorState;
typedef struct xpkEntry xpkEntry;
typedef struct xpkStorage xpkStorage;
typedef struct xpkWriteQueue xpkWriteQueue;


// 错误状态
struct xpkErrorState {
	int iCode;                      // 错误码
	char sText[XPK_ERROR_TEXT_CAP]; // 错误文本
};


// 运行时条目对象
struct xpkEntry {
	uint32_t iPos;              // 条目位置
	uint32_t iFlag;             // 条目标记
	uint32_t iFileHash;         // 文件哈希
	uint64_t iDataOffset;       // 数据偏移
	uint64_t iDataSize;         // 存储大小
	uint64_t iFileSize;         // 原始大小
	uint8_t bStored;            // 是否为原样存储

	int64_t iFileIndex;         // Index 模式下的索引号

	void* pInfoExt;             // Core 模式扩展信息
	char* sPath;                // Path 模式路径
	uint32_t iPlatformAttr;     // 平台属性
	uint64_t tCreateTime;       // 创建时间
	uint64_t tModifyTime;       // 修改时间
	uint64_t tAccessTime;       // 访问时间
};


// 运行时包对象
struct xpkStruct {
	xpkHead objHead;                // 当前包头快照

	uint8_t bReadonly;              // 是否只读
	uint8_t bBufferedDefault;       // 默认是否缓冲写入
	uint8_t bDirtyHead;             // 包头是否脏
	uint8_t bDirtyPackageMeta;      // 元数据是否脏
	uint8_t bDirtyEntryTable;       // 条目表是否脏
	uint8_t bDirtyData;             // 数据区是否脏
	uint8_t bSolidApplied;          // 已应用的 solid 布局
	uint8_t bVolumeApplied;         // 已应用的分卷布局
	uint16_t reserved0;             // 保留字段
	uint32_t iVolumeSizeApplied;    // 已应用的分卷大小

	uint64_t iAppendPos;            // 当前追加写入位置
	uint64_t iFileSize;             // 当前逻辑文件大小

	char* sPathPackage;             // 包路径
	void* pPackageMeta;             // 包元数据缓冲
	uint32_t iPackageMetaSize;      // 包元数据大小

	xarray_struct arrEntry;         // 条目数组
	xlist_struct lstEntry;          // 条目链表
	xdict_struct tblEntry;          // 路径查找表
	uint32_t iEntryCount;           // 条目数量

	xpkWriteQueue* pWriteQueue;     // 延迟写入队列
	xpkStorage* pStorage;           // 存储适配层
	xpkErrorState err;              // 对象级错误状态
};


// 脏位标记
typedef enum xpkDirtyBits {
	XPK_DIRTY_NONE = 0,
	XPK_DIRTY_HEAD = 1 << 0,
	XPK_DIRTY_PACKAGE_META = 1 << 1,
	XPK_DIRTY_ENTRY_TABLE = 1 << 2,
	XPK_DIRTY_DATA = 1 << 3
} xpkDirtyBits;


// 压缩级别映射
typedef struct xpkCompMap {
	uint32_t iAlgorithm;  // 目标算法
	int iNativeLevel;     // 对应原生级别
} xpkCompMap;


// 文件映射对象
typedef struct xpkMappedFile {
	const void* pView;    // 映射视图
	uint64_t iSize;       // 映射大小
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hMap;          // Windows 映射句柄
#else
	void* pMap;           // POSIX 映射句柄
#endif
} xpkMappedFile;


// 松散分卷扫描上下文
typedef struct xpkLooseVolumeScan {
	xpkObject objXpk;         // 包对象
	const char* sNameBase;    // 基础文件名
	size_t iNameSize;         // 基础文件名长度
	uint32_t iStartVolume;    // 起始分卷号
	uint32_t iCount;          // 命中数量
	int bDelete;              // 是否删除命中项
	int iError;               // 扫描过程错误码
} xpkLooseVolumeScan;


// 延迟写入节点
typedef struct xpkWriteNode {
	uint32_t iPos;            // 条目位置
	uint8_t iLevel;           // 压缩级别
	uint8_t reserved0[3];     // 保留字段
	uint32_t iCompSize;       // 压缩后大小
	uint64_t iRawSize;        // 原始大小
	void* pCompData;          // 压缩数据缓冲
} xpkWriteNode;


// 延迟写入队列
struct xpkWriteQueue {
	xarray_struct arrNode;    // 队列节点数组
};


// 文件写入分块大小
#define XPK_WRITE_FILE_CHUNK_SIZE (8u * 1024u * 1024u)

// 编解码流分块大小
#define XPK_CODEC_STREAM_CHUNK_SIZE (256u * 1024u)


// Flush 前的条目快照
typedef struct xpkFlushSnapshot {
	uint32_t iPos;            // 条目位置
	uint32_t iFlag;           // 条目标记
	uint64_t iDataOffset;     // 数据偏移
	uint64_t iDataSize;       // 存储大小
	uint64_t iFileSize;       // 原始大小
} xpkFlushSnapshot;


// 保存回滚上下文
typedef struct xpkSaveRollback {
	int bFileExisted;               // 保存前文件是否存在
	int bHeadValid;                 // 是否捕获到有效包头
	int bTailInFile;                // 尾段是否落到临时文件
	int bKeepTailFile;              // 是否保留尾段文件
	uint64_t iLogicalSize;          // 保存前逻辑大小
	uint64_t iDataOffset;           // 保存前数据起点
	uint64_t iTailSize;             // 尾段大小
	uint8_t sHeadBuf[XPK_HEAD_SIZE]; // 保存前包头镜像
	void* pTailData;                // 内存尾段缓冲
	char* sTailPath;                // 临时尾段路径
} xpkSaveRollback;


// 回滚内存上限
#define XPK_SAVE_ROLLBACK_MEM_LIMIT   (8u * 1024u * 1024u)

// 回滚文件分块大小
#define XPK_SAVE_ROLLBACK_CHUNK_SIZE  (8u * 1024u * 1024u)


// 构建阶段用 LZMA 输入流
typedef struct xpkBuildLzmaSeqIn {
	ISeqInStream vt;   // LZMA 输入接口
	xfile hFile;       // 源文件句柄
	uint64_t iRemain;  // 剩余字节数
} xpkBuildLzmaSeqIn;


// 构建阶段用 LZMA 输出流
typedef struct xpkBuildLzmaSeqOut {
	ISeqOutStream vt;  // LZMA 输出接口
	xfile hFile;       // 目标文件句柄
	uint64_t iSize;    // 已写大小
} xpkBuildLzmaSeqOut;


// 写入阶段用 LZMA 输入流
typedef struct xpkWriteLzmaSeqIn {
	ISeqInStream vt;   // LZMA 输入接口
	xfile hFile;       // 源文件句柄
	uint64_t iRemain;  // 剩余字节数
} xpkWriteLzmaSeqIn;


// 写入阶段用 LZMA 输出流
typedef struct xpkWriteLzmaSeqOut {
	ISeqOutStream vt;  // LZMA 输出接口
	xfile hFile;       // 目标文件句柄
	uint64_t iSize;    // 已写大小
} xpkWriteLzmaSeqOut;


// 写入阶段用 LZMA 内存输入流
typedef struct xpkWriteLzmaMemIn {
	ISeqInStream vt;      // LZMA 输入接口
	const uint8_t* pData; // 源数据缓冲
	uint64_t iSize;       // 源数据大小
	uint64_t iPos;        // 当前读取位置
} xpkWriteLzmaMemIn;


// 获取写入队列节点数量
static inline int procXpkWriteQueueCount(xpkObject objXpk);

// 释放写入队列
static inline void procXpkUnitWriteQueue(xpkObject objXpk);

// 标记对象为干净状态
static inline void procXpkMarkClean(xpkObject objXpk);

// 初始化默认包头
static inline void procXpkInitHead(xpkObject objXpk);

// 标记已应用布局
static inline void procXpkMarkAppliedLayout(xpkObject objXpk);

// 添加内存数据条目
static inline int procXpkAddDataEntry(xpkObject objXpk, xpkEntry* pEntrySeed, const void* pData, uint64_t iSize, const xpkWriteOptions* pOpt, uint32_t* pPosRet);

// 校验条目数量状态
static inline int procXpkValidateEntryCountState(xpkObject objXpk);

// 校验活动条目查找一致性
static inline int procXpkValidateLiveEntryLookup(xpkObject objXpk, const xpkEntry* pEntry);

// 仅追加构建后的条目描述
static inline int procXpkBuildAppendEntryOnly(xpkObject objDst, const xpkEntry* pEntrySrc, uint64_t iDataOffset, uint64_t iDataSize, uint64_t iFileSize);


// 获取 solid 原样存储级别
static inline uint8_t procXpkSolidStoredCompLevel(xpkObject objXpk);

// 计算 solid 原始数据大小
static inline int procXpkCalcSolidRawSize(xpkObject objXpk, uint64_t* pSizeRet);

// 计算映射文件范围哈希
static inline int procXpkHashMappedFileRange(xpkObject objXpk, xfile hFile, uint64_t iOffset, uint64_t iSize, uint32_t* pHashRet);

// 将 LZ4 解码块复制到文件
static inline int procXpkCopyDecodedLz4BlockToFile(xpkObject objXpk, uint8_t iLevel, const void* pCompData, uint32_t iCompSize, uint64_t iRawSize, const char* sPathFile);

// 立即写入原样文件
static inline int procXpkWriteImmediateStoreFile(xpkObject objXpk, xpkEntry* pEntry, const char* sSrcPath);

// 立即写入 LZ4 数据
static inline int procXpkWriteImmediateLz4Data(xpkObject objXpk, xpkEntry* pEntry, const void* pData, uint32_t iSize, uint8_t iLevel);

// 立即写入 ZSTD 数据
static inline int procXpkWriteImmediateZstdData(xpkObject objXpk, xpkEntry* pEntry, const void* pData, uint32_t iSize, uint8_t iLevel);

// 立即写入 LZMA2 数据
static inline int procXpkWriteImmediateLzma2Data(xpkObject objXpk, xpkEntry* pEntry, const void* pData, uint32_t iSize, uint8_t iLevel);

// 缓冲写入 LZ4 数据
static inline int procXpkWriteBufferedLz4Data(xpkObject objXpk, xpkEntry* pEntry, const void* pData, uint32_t iSize, uint8_t iLevel);

// 缓冲写入 ZSTD 数据
static inline int procXpkWriteBufferedZstdData(xpkObject objXpk, xpkEntry* pEntry, const void* pData, uint32_t iSize, uint8_t iLevel);

// 缓冲写入 LZMA2 数据
static inline int procXpkWriteBufferedLzma2Data(xpkObject objXpk, xpkEntry* pEntry, const void* pData, uint32_t iSize, uint8_t iLevel);


// 基于文件句柄校验原样条目
static inline int procXpkVerifyStoredEntryWithFile(xpkObject objXpk, xpkEntry* pEntry, xfile hFile);

// 基于映射校验原样条目
static inline int procXpkVerifyStoredEntryMapped(xpkObject objXpk, xpkEntry* pEntry, const xpkMappedFile* pMap);

// 基于文件句柄校验解码条目
static inline int procXpkVerifyDecodedEntryWithFile(xpkObject objXpk, xpkEntry* pEntry, xfile hFile);

// 基于映射校验解码条目
static inline int procXpkVerifyDecodedEntryMapped(xpkObject objXpk, xpkEntry* pEntry, const xpkMappedFile* pMap);

#endif

#endif
