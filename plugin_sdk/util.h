/* 应用级小工具：令牌生成、十六进制、数值与路径解析。
 * 全部为原生 xrt 组合，无 v1 语义。 */
#include <string.h>


/* xrtMap 键为字节视图；字符串键的便捷构造 */
static xbytesview KeyView(const char* s)
{
	return (xbytesview){(cbytes)(s ? s : ""), s ? strlen(s) : 0};
}
static xbytesview KeyViewN(const char* s, size_t n)
{
	return (xbytesview){(cbytes)(s ? s : ""), n};
}

static char* Util_HexUpper(const void* data, size_t size)
{
	return xrtHexEncodeNew(data, size, XHEX_UPPER);
}
static char* Util_Token(void)
{
	unsigned char data[16];
	return xrtSecureRandom(data, sizeof(data)) ? Util_HexUpper(data, sizeof(data)) : NULL;
}
static int64 Util_ParseI64(const char* text)
{
	int64 value = 0;
	if (text) (void)xrtIntParse(xrtStrView(text), 10, XNUMBER_PARSE_SPACE, &value);
	return value;
}
/* 扩展名不含前导点；无扩展名返回空串。 */
static char* Util_ExtNoDot(const char* path)
{
	char* ext = xrtPathExt(path);
	if (ext && ext[0] == '.') memmove(ext, ext + 1, strlen(ext));
	return ext;
}

/* 目录遍历：proc 返回非 0 停止；递归发生在目录条目之后。 */
typedef int (*DirVisitProc)(const char* path, size_t size, bool directory, void* context);
static void DirScan(const char* path, bool recursive, DirVisitProc proc, void* context)
{
	xdir dir = xrtDirOpen(path, XDIR_STAT);
	xdirentry entry;
	if (!dir) return;
	while (xrtDirNext(dir, &entry) == XDIR_NEXT_ITEM) {
		char* full = xrtDirEntryPath(dir, &entry);
		bool directory = entry.Info.Type == XFILE_TYPE_DIRECTORY;
		int stop = full ? proc(full, (size_t)entry.Info.Size, directory, context) : 0;
		if (!stop && recursive && directory) DirScan(full, true, proc, context);
		xrtFree(full);
		if (stop) break;
	}
	xrtDirClose(dir);
}
