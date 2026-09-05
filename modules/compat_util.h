/* 与 v1 持久化格式有关的小适配，不在业务代码中散布单位/编码转换。 */
#define XRT_TIME_FORMAT_DATETIME 0
#define XRT_TIME_FORMAT_DATE 1
#define XRT_TIME_FORMAT_TIME 2
#define XRT_CP_BINARY 0
#define XA_V1_EPOCH INT64_C(62167219200)

/* v1 数据库保存从公元零年起算的本地日历秒；新版 xtime 是 Unix 微秒。
 * 不重写历史记录，业务 SQL 继续读写原单位。框架定时器仍使用新版单位。 */
static xtime XA_Now(void)
{
	xdatetime date; xtime wall;
	if (!xrtTimeLocal(xrtNow(), &date)) return 0;
	date.Offset = 0;
	if (!xrtTimeMake(&date, &wall)) return 0;
	return xrtTimeUnix(wall) + XA_V1_EPOCH;
}
static char* XA_TimeToStr(int64 old, int format)
{
	xtime time;
	const char* fmt = format == 1 ? "%Y-%m-%d" : format == 2 ? "%H:%M:%S" : "%Y-%m-%d %H:%M:%S";
	if (!xrtTimeFromUnix(old - XA_V1_EPOCH, &time)) return xrtStrDup("");
	return xrtTimeFormat(time, 0, xrtStrView(fmt));
}
static int64 XA_StrToTime(const char* text, size_t size)
{
	xtime time;
	if (!text || !xrtTimeParseAny(XA_Key(text, size), &time)) return 0;
	return xrtTimeUnix(time) + XA_V1_EPOCH;
}
static char* xrtCopyStr(const char* text, size_t size)
{
	return text ? xrtStrDupView(XA_Key(text, size)) : NULL;
}
static int64 xrtStrToI64(const char* text)
{
	int64 value = 0;
	if (text) xrtIntParse(xrtStrView(text), 10, XNUMBER_PARSE_SPACE, &value);
	return value;
}
static int xrtI64ToStr(int64 value, char* out) { return sprintf(out, "%lld", (long long)value); }
/* v1 服务端哈希使用大写十六进制；客户端哈希则保持页面传入的原样。 */
static char* XA_HexEncode(const void* data, size_t size) { return xrtHexEncodeNew(data, size, XHEX_UPPER); }
static char* xrtMakeXIDS(void)
{
	unsigned char data[16];
	return xrtSecureRandom(data, sizeof(data)) ? XA_HexEncode(data, sizeof(data)) : NULL;
}
static char* XA_PathJoin(int count, ...)
{
	va_list args; char* path; int i;
	if (count <= 0) return NULL;
	va_start(args, count);
	path = xrtStrDup(va_arg(args, const char*));
	for (i = 1; i < count && path; ++i) {
		char* next = xrtPathJoin(path, va_arg(args, const char*));
		xrtFree(path); path = next;
	}
	va_end(args);
	return path;
}
static char* xrtPathGetExt(const char* path, size_t size)
{
	char* ext = xrtPathExt(path); (void)size;
	if (ext && ext[0] == '.') memmove(ext, ext + 1, strlen(ext));
	return ext;
}
static char* xrtPathGetName(const char* path, size_t size) { (void)size; return xrtPathStem(path); }
static int xrtStrComp(const char* a, const char* b, size_t size, bool exact)
{
	xstrview left = xrtStrViewN(a, strlen(a) < size ? strlen(a) : size);
	xstrview right = xrtStrViewN(b, strlen(b) < size ? strlen(b) : size);
	return exact ? xrtStrCompare(left, right) : xrtStrCaseCompare(left, right);
}
static char* XA_FileReadAll(const char* path, int encoding, size_t* size)
{
	(void)encoding;
	return (char*)xrtFileReadAll(path, size);
}
static xvalue* xrtParseJSON_File(const char* path)
{
	size_t size = 0; char* text = (char*)xrtFileReadAll(path, &size);
	xvalue* result = text ? xrtJsonParse(xrtStrViewN(text, size)) : NULL;
	xrtFree(text);
	return result;
}
static bool xrtStringifyJSON_File(const char* path, xvalue* value, bool pretty)
{
	size_t size = 0; char* text = xrtJsonStringify(value, pretty, &size);
	bool ok = text && xrtFileWriteAtomic(path, (xbytesview){(cbytes)text, size});
	xrtFree(text);
	return ok;
}
typedef int (*XA_DirProc)(char*, size_t, int, void*, void*);
static void xrtDirScan(const char* path, bool recursive, XA_DirProc proc, void* context)
{
	xdir dir = xrtDirOpen(path, XDIR_STAT); xdirentry entry;
	if (!dir) return;
	while (xrtDirNext(dir, &entry) == XDIR_NEXT_ITEM) {
		char* full = xrtDirEntryPath(dir, &entry);
		bool directory = entry.Info.Type == XFILE_TYPE_DIRECTORY;
		int stop = proc(full, (size_t)entry.Info.Size, directory, NULL, context);
		if (!stop && recursive && directory) xrtDirScan(full, true, proc, context);
		xrtFree(full);
		if (stop) break;
	}
	xrtDirClose(dir);
}
