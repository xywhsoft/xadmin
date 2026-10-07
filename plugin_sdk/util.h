/* 应用级小工具：令牌生成、十六进制、数值与路径解析。
 * 全部为原生 xrt 组合，无 v1 语义。 */
#include <string.h>
#include <stdio.h> /* snprintf（限流槽位写入） */


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

/* M6：轻量每 IP 固定窗口限流（业务区单线程执行，无并发竞争）。
 * 超限返回 false；窗口=60s。槽位有限，未命中按最旧窗口复用。 */
#define UTIL_RATE_SLOTS 64
static struct { char ip[48]; int64 windowStart; int count; } Util_RateSlots[UTIL_RATE_SLOTS];
static bool Util_RateAllow(const char* ip, int maxPerMinute)
{
	int64 now = XAdmin_UnixNowUs();
	int64 window = 60LL * 1000000LL;
	int oldest = 0;
	int i;
	if (ip == NULL || ip[0] == '\0') ip = "?";
	for (i = 0; i < UTIL_RATE_SLOTS; i++) {
		if (Util_RateSlots[i].ip[0] != '\0' && strcmp(Util_RateSlots[i].ip, ip) == 0) {
			if (now - Util_RateSlots[i].windowStart >= window) {
				Util_RateSlots[i].windowStart = now;
				Util_RateSlots[i].count = 1;
				return maxPerMinute >= 1;
			}
			Util_RateSlots[i].count++;
			return Util_RateSlots[i].count <= maxPerMinute;
		}
		if (Util_RateSlots[i].ip[0] == '\0') oldest = i;
		if (Util_RateSlots[i].windowStart < Util_RateSlots[oldest].windowStart) oldest = i;
	}
	snprintf(Util_RateSlots[oldest].ip, sizeof(Util_RateSlots[oldest].ip), "%s", ip);
	Util_RateSlots[oldest].windowStart = now;
	Util_RateSlots[oldest].count = 1;
	return maxPerMinute >= 1;
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

/* L9：LIKE 模式通配符转义（\ % _ → \\ \% \_），
 * 配合 SQL 的 ESCAPE '\\' 子句。防止用户搜索串注入 %/_ 造成全表扫。
 * 失败（容量不足）返回 false。 */
static bool Util_LikeEscape(const char* in, char* out, size_t cap)
{
	size_t w = 0;
	if (!in || !out || cap < 3) return false;
	for (; *in; in++) {
		if (w + 2 >= cap) { out[0] = 0; return false; }
		if (*in == '\\' || *in == '%' || *in == '_') out[w++] = '\\';
		out[w++] = *in;
	}
	out[w] = 0;
	return true;
}
