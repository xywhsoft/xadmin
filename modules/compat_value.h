/* v1 的值操作适配层。
 * 所有值都是新版 xrt 的原生 xvalue*；不复制旧对象布局或旧运行时。
 * Get 返回借用值；colloc=true 消费调用方的引用/字符串，保持旧业务约定。
 * 可变缓存由应用请求锁保护，不再触碰旧版 Owner/AVLT 内部字段。
 */
#define TRUE true
#define FALSE false
#define XRT_OBJMODE_SHARED 0
#define XVO_DT_NULL XVALUE_NULL
#define XVO_DT_BOOL XVALUE_BOOL
#define XVO_DT_INT XVALUE_INT
#define XVO_DT_FLOAT XVALUE_FLOAT
#define XVO_DT_TEXT XVALUE_STRING
#define XVO_DT_TIME XVALUE_TIME
#define XVO_DT_ARRAY XVALUE_ARRAY
#define XVO_DT_TABLE XVALUE_OBJECT
#define XVO_DT_LIST XVALUE_INT_MAP
#define XVO_DT_COLL XVALUE_SET
#define xvoType xrtValueType
#define xvoAddRef xrtValueRetain
#define xvoUnref xrtValueRelease
#define xvoCreateNull xrtValueNull
#define xvoCreateBool xrtValueBool
#define xvoCreateInt xrtValueInt
#define xvoCreateFloat xrtValueFloat
#define xvoCreateTime xrtValueTime
#define xvoCreateTable xrtValueObject
#define xvoCreateArray xrtValueArray
#define xvoCreateList xrtValueIntMap
#define xvoCreateTableEx(mode) xrtValueObject()
#define xvoCreateArrayEx(mode) xrtValueArray()
#define xvoCreateListEx(mode) xrtValueIntMap()
#define xvoArrayGetValue xrtValueArrayGet
#define xvoListGetValue xrtValueIntMapGet
#define xvoArrayItemCount xrtValueCount
#define xvoTableItemCount xrtValueCount
#define xvoListItemCount xrtValueCount
#define xvoArraySwap xrtValueArraySwap
#define xvoCopy xrtValueDeepClone
#define xvoDeepCopy xrtValueDeepClone

static xstrview XA_Key(const char* key, size_t len)
{
	return xrtStrViewN(key ? key : "", key ? (len ? len : strlen(key)) : 0);
}
static char* xvoGetText(const xvalue* value)
{
	xstrview text = {0};
	return xrtValueGetString(value, &text) ? (char*)text.Data : NULL;
}
static int64 xvoGetInt(const xvalue* value)
{
	int64 result = 0;
	(void)xrtValueGetInt(value, &result);
	return result;
}
static bool xvoGetBool(const xvalue* value)
{
	bool result = false;
	(void)xrtValueGetBool(value, &result);
	return result;
}
static double xvoGetFloat(const xvalue* value)
{
	double result = 0;
	(void)xrtValueGetFloat(value, &result);
	return result;
}
static xtime xvoGetTime(const xvalue* value)
{
	xtime result = 0;
	(void)xrtValueGetTime(value, &result);
	return result;
}
static xvalue* xvoCreateText(char* text, size_t size, bool take)
{
	xvalue* value = xrtValueString(XA_Key(text, size));
	if (take) xrtFree(text);
	return value;
}
static xvalue* xvoTableGetValue(const xvalue* obj, const char* key, size_t len)
{
	return xrtValueObjectGet(obj, XA_Key(key, len));
}
static char* xvoTableGetText(const xvalue* obj, const char* key, size_t len)
{
	return xvoGetText(xvoTableGetValue(obj, key, len));
}
static int64 xvoTableGetInt(const xvalue* obj, const char* key, size_t len)
{
	return xvoGetInt(xvoTableGetValue(obj, key, len));
}
static bool xvoTableGetBool(const xvalue* obj, const char* key, size_t len)
{
	return xvoGetBool(xvoTableGetValue(obj, key, len));
}
static int xvoTableItemType(const xvalue* obj, const char* key, size_t len)
{
	return xrtValueType(xvoTableGetValue(obj, key, len));
}
static bool xvoTableExists(const xvalue* obj, const char* key, size_t len)
{
	return xrtValueObjectHas(obj, XA_Key(key, len));
}
static bool xvoTableSetValue(xvalue* obj, const char* key, size_t len, xvalue* value, bool take)
{
	return take ? xrtValueObjectSetNew(obj, XA_Key(key, len), value)
	            : xrtValueObjectSet(obj, XA_Key(key, len), value);
}
static bool xvoTableSetText(xvalue* obj, const char* key, size_t len, char* text, size_t size, bool take)
{
	return xrtValueObjectSetNew(obj, XA_Key(key, len), xvoCreateText(text, size, take));
}
static bool xvoTableSetInt(xvalue* obj, const char* key, size_t len, int64 value)
{
	return xrtValueObjectSetNew(obj, XA_Key(key, len), xrtValueInt(value));
}
static bool xvoTableSetBool(xvalue* obj, const char* key, size_t len, bool value)
{
	return xrtValueObjectSetNew(obj, XA_Key(key, len), xrtValueBool(value));
}
static bool xvoArrayAppendValue(xvalue* arr, xvalue* value, bool take)
{
	return take ? xrtValueArrayAppendNew(arr, value) : xrtValueArrayAppend(arr, value);
}
static bool xvoArrayAppendInt(xvalue* arr, int64 value)
{
	return xrtValueArrayAppendNew(arr, xrtValueInt(value));
}
static bool xvoArrayAppendText(xvalue* arr, char* text, size_t size, bool take)
{
	return xrtValueArrayAppendNew(arr, xvoCreateText(text, size, take));
}
static int64 xvoArrayGetInt(const xvalue* arr, size_t i) { return xvoGetInt(xrtValueArrayGet(arr, i)); }
static char* xvoArrayGetText(const xvalue* arr, size_t i) { return xvoGetText(xrtValueArrayGet(arr, i)); }
static int64 xvoListGetInt(const xvalue* arr, int64 i) { return xvoGetInt(xrtValueIntMapGet(arr, i)); }
static bool xvoListGetBool(const xvalue* arr, int64 i) { return xvoGetBool(xrtValueIntMapGet(arr, i)); }
static bool xvoListSetInt(xvalue* arr, int64 i, int64 v) { return xrtValueIntMapSetNew(arr, i, xrtValueInt(v)); }
static bool xvoListSetBool(xvalue* arr, int64 i, bool v) { return xrtValueIntMapSetNew(arr, i, xrtValueBool(v)); }
static bool xvoListSetValue(xvalue* arr, int64 i, xvalue* v, bool take)
{
	return take ? xrtValueIntMapSetNew(arr, i, v) : xrtValueIntMapSet(arr, i, v);
}
static xvalue* xrtParseJSON(const char* text, size_t size)
{
	return text ? xrtJsonParse(XA_Key(text, size)) : NULL;
}
#define xrtStringifyJSON xrtJsonStringify

/* 旧遍历回调协议仅做桥接，容器本身仍使用 xrt map/value。 */
typedef xmap* xdict;
typedef struct { char* Key; uint32 KeyLen; } Dict_Key;
typedef bool (*XA_DictProc)(Dict_Key*, void*, void*);
static xdict XA_DictCreate(size_t size, int mode) { (void)mode; return xrtMapCreate(size); }
static void* XA_DictGet(xdict map, const char* key, size_t n)
{
	return xrtMapGet(map, (xbytesview){(cbytes)key, n});
}
static void* XA_DictSet(xdict map, const char* key, size_t n, bool* added)
{
	return xrtMapGetOrAdd(map, (xbytesview){(cbytes)key, n}, added);
}
static bool XA_DictRemove(xdict map, const char* key, size_t n)
{
	return xrtMapRemove(map, (xbytesview){(cbytes)key, n});
}
static void XA_DictWalk(xdict map, XA_DictProc proc, void* context)
{
	xmapiter it = {0}; xbytesview key; void* value;
	if (!xrtMapIterBegin(map, &it)) return;
	while ((value = xrtMapIterNext(&it, &key)) != NULL) {
		Dict_Key oldKey = {(char*)key.Data, (uint32)key.Size};
		if (proc(&oldKey, value, context)) break;
	}
	xrtMapIterEnd(&it);
}
static void XA_ValueWalk(xvalue* obj, XA_DictProc proc, void* context)
{
	xvalueiter it = {0}; xvaluekey key; xvalue* value;
	if (!xrtValueIterBegin(obj, &it)) return;
	while ((value = xrtValueIterNext(&it, &key)) != NULL) {
		Dict_Key oldKey = {(char*)key.String.Data, (uint32)key.String.Size};
		if (key.Type == XVALUE_KEY_STRING && proc(&oldKey, &value, context)) break;
	}
	xrtValueIterEnd(&it);
}
