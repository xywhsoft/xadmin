/* 原生 xrtValue 便捷层：仅消除双步取值与 xstrview 样板。
 * 语义与 xrt 完全一致——Get 系返回借用视图，SetOwn 系消费调用方引用，
 * SetText 系拷贝文本；无 keylen、无 take 布尔双义、无 v1 单位。 */
#include <string.h>


#define TIME_TEXT_DATETIME 0
#define TIME_TEXT_DATE     1
#define TIME_TEXT_CLOCK    2

static xvalue* ValueObject(void)
{
	return xrtValueObject();
}
static xvalue* ValueArray(void)
{
	return xrtValueArray();
}
static xvalue* ValueGet(const xvalue* obj, const char* key)
{
	return key ? xrtValueObjectGet(obj, xrtStrView(key)) : NULL;
}
static str ValueText(const xvalue* obj, const char* key)
{
	xstrview text = {0};
	xvalue* value = ValueGet(obj, key);
	if (!value || !xrtValueGetString(value, &text)) return NULL;
	return (str)text.Data;
}
static int64 ValueInt(const xvalue* obj, const char* key)
{
	int64 result = 0;
	xvalue* value = ValueGet(obj, key);
	if (value) (void)xrtValueGetInt(value, &result);
	return result;
}
static bool ValueBool(const xvalue* obj, const char* key)
{
	bool result = false;
	xvalue* value = ValueGet(obj, key);
	if (value) (void)xrtValueGetBool(value, &result);
	return result;
}
static bool ValueHas(const xvalue* obj, const char* key)
{
	return key && xrtValueObjectHas(obj, xrtStrView(key));
}
static bool ValueSetOwn(xvalue* obj, const char* key, xvalue* value)
{
	return xrtValueObjectSetNew(obj, xrtStrView(key), value);
}
static bool ValueSetRef(xvalue* obj, const char* key, const xvalue* value)
{
	return xrtValueObjectSet(obj, xrtStrView(key), value);
}
static bool ValueSetText(xvalue* obj, const char* key, const char* text)
{
	return xrtValueObjectSetNew(obj, xrtStrView(key),
		xrtValueString(text ? xrtStrView(text) : xrtStrView("")));
}
static bool ValueSetOwnedText(xvalue* obj, const char* key, char* text)
{
	bool ok = ValueSetText(obj, key, text);
	if (text) xrtFree(text);
	return ok;
}
static bool ValueSetInt(xvalue* obj, const char* key, int64 value)
{
	return xrtValueObjectSetNew(obj, xrtStrView(key), xrtValueInt(value));
}
static bool ValueSetFloat(xvalue* obj, const char* key, double value)
{
	return xrtValueObjectSetNew(obj, xrtStrView(key), xrtValueFloat(value));
}
static bool ValueSetBool(xvalue* obj, const char* key, bool value)
{
	return xrtValueObjectSetNew(obj, xrtStrView(key), xrtValueBool(value));
}
static bool ValueArrayOwn(xvalue* arr, xvalue* value)
{
	return xrtValueArrayAppendNew(arr, value);
}
static bool ValueArrayRef(xvalue* arr, const xvalue* value)
{
	return xrtValueArrayAppend(arr, value);
}
static size_t ValueCount(const xvalue* container)
{
	return xrtValueCount(container);
}
static str ValueArrayText(const xvalue* arr, size_t i)
{
	xstrview text = {0};
	xvalue* value = xrtValueArrayGet(arr, i);
	if (!value || !xrtValueGetString(value, &text)) return NULL;
	return (str)text.Data;
}
static int64 ValueArrayInt(const xvalue* arr, size_t i)
{
	int64 result = 0;
	xvalue* value = xrtValueArrayGet(arr, i);
	if (value) (void)xrtValueGetInt(value, &result);
	return result;
}

/* 标量值本体取值（非表键访问） */
static str ValueTextOf(const xvalue* value)
{
	xstrview text = {0};
	if (!value || !xrtValueGetString(value, &text)) return NULL;
	return (str)text.Data;
}
static int64 ValueIntOf(const xvalue* value)
{
	int64 result = 0;
	if (value) (void)xrtValueGetInt(value, &result);
	return result;
}
static bool ValueBoolOf(const xvalue* value)
{
	bool result = false;
	if (value) (void)xrtValueGetBool(value, &result);
	return result;
}
static double ValueFloatOf(const xvalue* value)
{
	double result = 0;
	if (value) (void)xrtValueGetFloat(value, &result);
	return result;
}
static xtime ValueTimeOf(const xvalue* value)
{
	xtime result = 0;
	if (value) (void)xrtValueGetTime(value, &result);
	return result;
}

/* 列表（整型键映射）便捷层，语义同表访问 */
static xvalue* ValueMapGet(const xvalue* map, int64 key)
{
	return xrtValueIntMapGet(map, key);
}
static int64 ValueMapInt(const xvalue* map, int64 key)
{
	return ValueIntOf(xrtValueIntMapGet(map, key));
}
static bool ValueMapBool(const xvalue* map, int64 key)
{
	return ValueBoolOf(xrtValueIntMapGet(map, key));
}
static bool ValueMapOwn(xvalue* map, int64 key, xvalue* value)
{
	return xrtValueIntMapSetNew(map, key, value);
}
static bool ValueMapRef(xvalue* map, int64 key, const xvalue* value)
{
	return xrtValueIntMapSet(map, key, value);
}
static bool ValueMapSetInt(xvalue* map, int64 key, int64 value)
{
	return xrtValueIntMapSetNew(map, key, xrtValueInt(value));
}
static bool ValueMapSetBool(xvalue* map, int64 key, bool value)
{
	return xrtValueIntMapSetNew(map, key, xrtValueBool(value));
}

/* JSON 解析的计数串入口；size 为 0 时按 C 字符串取全长。 */
static xvalue* JsonParseN(const char* text, size_t size)
{
	if (!text) return NULL;
	return xrtJsonParse(size ? xrtStrViewN(text, size) : xrtStrView(text));
}
static xvalue* JsonParseFile(const char* path)
{
	size_t size = 0;
	char* text = (char*)xrtFileReadAll(path, &size);
	xvalue* result = text ? xrtJsonParse(xrtStrViewN(text, size)) : NULL;
	xrtFree(text);
	return result;
}
static bool JsonWriteFile(const char* path, xvalue* value, bool pretty)
{
	size_t size = 0;
	char* text = xrtJsonStringify(value, pretty, &size);
	bool ok = text && xrtFileWriteAtomic(path, (xbytesview){(cbytes)text, size});
	xrtFree(text);
	return ok;
}

/* map / value 原生迭代器的回调遍历（早退即停） */
typedef bool (*MapWalkProc)(xbytesview key, void* value, void* context);
static void MapWalk(const xmap* map, MapWalkProc proc, void* context)
{
	xmapiter it = {0};
	xbytesview key;
	void* value;
	if (!xrtMapIterBegin((xmap*)map, &it)) return;
	while ((value = xrtMapIterNext(&it, &key)) != NULL) {
		if (proc(key, value, context)) break;
	}
	xrtMapIterEnd(&it);
}
typedef bool (*ValueWalkProc)(xstrview key, xvalue* value, void* context);
static void ValueWalk(xvalue* obj, ValueWalkProc proc, void* context)
{
	xvalueiter it = {0};
	xvaluekey key;
	xvalue* value;
	if (!xrtValueIterBegin(obj, &it)) return;
	while ((value = xrtValueIterNext(&it, &key)) != NULL) {
		if (key.Type == XVALUE_KEY_STRING && proc(key.String, value, context)) break;
	}
	xrtValueIterEnd(&it);
}

/* 本地时区格式化；返回堆串（xrtFree）。 */
static str TimeText(xtime t, int fmt)
{
	xdatetime date;
	if (!xrtTimeLocal(t, &date)) return xrtStrDup("");
	return xrtDateTimeFormat(&date, xrtStrView(
		fmt == TIME_TEXT_DATE ? "%Y-%m-%d" :
		fmt == TIME_TEXT_CLOCK ? "%H:%M:%S" : "%Y-%m-%d %H:%M:%S"));
}
