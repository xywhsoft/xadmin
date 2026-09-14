# -*- coding: utf-8 -*-
"""v1 方言插件 → 原生 xs3 方言 机械变换（方案 A 插件侧，一次性工具）。
与 native_convert.py 同规则集 + 插件特有形态（multipart 桩、int64 键 dict、
文件信息包装）。输出后由 probe_plugins.py 编译加载门禁验证。"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RULES = [
    # 值创建/引用
    (r'\bxvoCreateTableEx\([^)]*\)', 'ValueObject()'),
    (r'\bxvoCreateArrayEx\([^)]*\)', 'ValueArray()'),
    (r'\bxvoCreateListEx\([^)]*\)', 'xrtValueIntMap()'),
    (r'\bxvoCreateTable\(\)', 'ValueObject()'),
    (r'\bxvoCreateArray\(\)', 'ValueArray()'),
    (r'\bxvoCreateList\(\)', 'xrtValueIntMap()'),
    (r'\bxvoCreateNull\(\)', 'xrtValueNull()'),
    (r'\bxvoCreateBool\(', 'xrtValueBool('),
    (r'\bxvoCreateInt\(', 'xrtValueInt('),
    (r'\bxvoCreateFloat\(', 'xrtValueFloat('),
    (r'\bxvoCreateTime\(', 'xrtValueTime('),
    (r'\bxvoUnref\(', 'xrtValueRelease('),
    (r'\bxvoAddRef\(', 'xrtValueRetain('),
    (r'\bxvoDeepCopy\(', 'xrtValueDeepClone('),
    (r'\bxvoCopy\(', 'xrtValueDeepClone('),
    (r'\bxvoType\(', 'xrtValueType('),
    (r'\bxvoGetText\(', 'ValueTextOf('),
    (r'\bxvoGetInt\(', 'ValueIntOf('),
    (r'\bxvoGetBool\(', 'ValueBoolOf('),
    (r'\bxvoGetFloat\(', 'ValueFloatOf('),
    (r'\bxvoGetTime\(', 'ValueTimeOf('),
    # 表 Get
    (r'\bxvoTableGetValue\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueGet(\1, \2)'),
    (r'\bxvoTableGetText\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueText(\1, \2)'),
    (r'\bxvoTableGetInt\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueInt(\1, \2)'),
    (r'\bxvoTableGetBool\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueBool(\1, \2)'),
    (r'\bxvoTableItemType\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'xrtValueType(ValueGet(\1, \2))'),
    (r'\bxvoTableExists\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueHas(\1, \2)'),
    # 表 Set（take 双义）
    (r'\bxvoTableSetValue\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*TRUE\)', r'ValueSetOwn(\1, \2, \3)'),
    (r'\bxvoTableSetValue\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*FALSE\)', r'ValueSetRef(\1, \2, \3)'),
    (r'\bxvoTableSetText\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*0,\s*TRUE\)', r'ValueSetOwnedText(\1, \2, \3)'),
    (r'\bxvoTableSetText\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*0,\s*FALSE\)', r'ValueSetText(\1, \2, \3)'),
    (r'\bxvoTableSetInt\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*((?:[^()]|\([^()]*\))+)\)', r'ValueSetInt(\1, \2, \3)'),
    (r'\bxvoTableSetBool\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*((?:[^()]|\([^()]*\))+)\)', r'ValueSetBool(\1, \2, \3)'),
    # 数组/列表
    (r'\bxvoArrayAppendValue\(([^,()]+),\s*(.+),\s*TRUE\)', r'ValueArrayOwn(\1, \2)'),
    (r'\bxvoArrayAppendValue\(([^,()]+),\s*(.+),\s*FALSE\)', r'ValueArrayRef(\1, \2)'),
    (r'\bxvoArrayAppendInt\(([^,()]+),\s*(.+)\)', r'xrtValueArrayAppendNew(\1, xrtValueInt(\2))'),
    (r'\bxvoArrayAppendText\(([^,()]+),\s*(.+),\s*[^,()]+,\s*TRUE\)', r'ValueArrayOwn(\1, xrtValueString(xrtStrView(\2)))'),
    (r'\bxvoArrayAppendText\(([^,()]+),\s*(.+),\s*[^,()]+,\s*FALSE\)', r'ValueArrayRef(\1, xrtValueString(xrtStrView(\2)))'),
    (r'\bxvoArrayGetValue\(', 'xrtValueArrayGet('),
    (r'\bxvoArrayGetText\(', 'ValueArrayText('),
    (r'\bxvoArrayGetInt\(', 'ValueArrayInt('),
    (r'\bxvoArrayItemCount\(', 'ValueCount('),
    (r'\bxvoTableItemCount\(', 'ValueCount('),
    (r'\bxvoListItemCount\(', 'ValueCount('),
    (r'\bxvoListGetValue\(', 'ValueMapGet('),
    (r'\bxvoListGetInt\(', 'ValueMapInt('),
    (r'\bxvoListGetBool\(', 'ValueMapBool('),
    (r'\bxvoListSetValue\(([^,()]+),\s*([^,()]+),\s*(.+),\s*TRUE\)', r'ValueMapOwn(\1, \2, \3)'),
    (r'\bxvoListSetValue\(([^,()]+),\s*([^,()]+),\s*(.+),\s*FALSE\)', r'ValueMapRef(\1, \2, \3)'),
    (r'\bxvoListSetInt\(', 'ValueMapSetInt('),
    (r'\bxvoListSetBool\(', 'ValueMapSetBool('),
    # JSON / 时间 / 字符串
    (r'\bxrtStringifyJSON\(', 'xrtJsonStringify('),
    (r'\bxrtParseJSON\(', 'JsonParseN('),
    (r'\bXA_Now\(\)', 'xrtNow()'),
    (r'\bXA_TimeToStr\((.+?),\s*XRT_TIME_FORMAT_DATETIME\)', r'TimeText(\1, TIME_TEXT_DATETIME)'),
    (r'\bXA_TimeToStr\((.+?),\s*XRT_TIME_FORMAT_DATE\)', r'TimeText(\1, TIME_TEXT_DATE)'),
    (r'\bXA_TimeToStr\((.+?),\s*XRT_TIME_FORMAT_TIME\)', r'TimeText(\1, TIME_TEXT_CLOCK)'),
    (r'\bxrtCopyStr\(((?:[^()]|\([^()]*\))+),\s*0\)', r'xrtStrDup(\1)'),
    (r'\bxrtStrToI64\(', 'Util_ParseI64('),
    (r'\bxrtMakeXIDS\(', 'Util_Token('),
    (r'\bxrtPathGetExt\(([^,()]+),\s*[^,()]+\)', r'Util_ExtNoDot(\1)'),
    (r'\bxrtPathGetExt\(([^,()]+)\)', r'Util_ExtNoDot(\1)'),
    # 请求别名
    (r'\bxsReqBody\(', 'XAdmin_ReqBody('),
    (r'\bxsReqBodyLen\(', 'XAdmin_ReqBodyLen('),
    (r'\bXHTTPD_METHOD_GET\b', 'XHTTP_METHOD_GET'),
    (r'\bXHTTPD_METHOD_POST\b', 'XHTTP_METHOD_POST'),
    (r'\bXHTTPD_METHOD_PUT\b', 'XHTTP_METHOD_PUT'),
    (r'\bXHTTPD_METHOD_PATCH\b', 'XHTTP_METHOD_PATCH'),
    (r'\bXHTTPD_METHOD_DELETE\b', 'XHTTP_METHOD_DELETE'),
    (r'\bXHTTPD_METHOD_HEAD\b', 'XHTTP_METHOD_HEAD'),
    # 类型常量
    (r'\bXVO_DT_NULL\b', 'XVALUE_NULL'),
    (r'\bXVO_DT_BOOL\b', 'XVALUE_BOOL'),
    (r'\bXVO_DT_INT\b', 'XVALUE_INT'),
    (r'\bXVO_DT_FLOAT\b', 'XVALUE_FLOAT'),
    (r'\bXVO_DT_TEXT\b', 'XVALUE_STRING'),
    (r'\bXVO_DT_TIME\b', 'XVALUE_TIME'),
    (r'\bXVO_DT_ARRAY\b', 'XVALUE_ARRAY'),
    (r'\bXVO_DT_TABLE\b', 'XVALUE_OBJECT'),
    (r'\bXVO_DT_LIST\b', 'XVALUE_INT_MAP'),
    (r'\bXVO_DT_COLL\b', 'XVALUE_SET'),
    # 布尔
    (r'\bTRUE\b', 'true'),
    (r'\bFALSE\b', 'false'),
]

# xlogserver：int64 二进制键 dict → xrtMap 字节视图键
DICT_RULES = [
    (r'xrtDictCreate\(sizeof\(sqlite3\*\), XRT_OBJMODE_SHARED\)', 'xrtMapCreate(sizeof(sqlite3*))'),
    (r'xrtDictGetPtr\(([^,()]+), \(str\)&(\w+), sizeof\(int64\)\)',
     r'xrtMapGet(\1, (xbytesview){(cbytes)&\2, sizeof(int64)})'),
    (r'xrtDictSetPtr\(([^,()]+), \(str\)&(\w+), sizeof\(int64\), (\w+), NULL\)',
     r'(*(sqlite3**)xrtMapGetOrAdd(\1, (xbytesview){(cbytes)&\2, sizeof(int64)}, NULL) = \3)'),
    (r'xrtDictWalk\(', 'MapWalk('),
    (r'xrtDictDestroy\(', 'xrtMapDestroy('),
]

MULTIPART_STUB = '''
/* multipart 本地桩：宿主尚未接入 xrt extlibs 解码器；上传路由走错误分支。 */
typedef struct { const char* sPtr; size_t iLen; } xrtmultipartboundaryview;
typedef struct xrtmultipartpartview {
	struct { const char* sPtr; size_t iLen; } tName;
	struct { const char* sPtr; size_t iLen; } tBody;
} xrtmultipartpartview;
static bool xrtMultipartBoundaryFromContentType(const char* sContentType, xrtmultipartboundaryview* pBoundary)
{
	const char* sB;
	if (!sContentType || !pBoundary) return false;
	sB = strstr(sContentType, "boundary=");
	if (!sB) return false;
	sB += 9;
	pBoundary->sPtr = sB;
	pBoundary->iLen = strlen(sB);
	return pBoundary->iLen > 0;
}
static bool xrtMultipartNextN(const char* sBody, size_t iBodyLen, const char* sBoundary, size_t iBoundaryLen, size_t* pOffset, xrtmultipartpartview* pPart)
{
	(void)sBody; (void)iBodyLen; (void)sBoundary; (void)iBoundaryLen; (void)pOffset; (void)pPart;
	return false;
}
static bool xrtMultipartDecodeFileNameTo(const xrtmultipartpartview* pPart, char* sOut, size_t iOutSize, size_t* pNameLen)
{
	(void)pPart; (void)sOut; (void)iOutSize; (void)pNameLen;
	return false;
}
/* 路径式文件信息（xrt 原生为句柄式） */
static int64 xrtFileGetSize(const char* path)
{
	xfile f; uint64 size = 0;
	if (!path) return 0;
	f = xrtFileOpen(path, &((xfileoptions){.Flags = XFILE_READ}));
	if (!f) return 0;
	xrtFileSize(f, &size);
	xrtClose(f);
	return (int64)size;
}
static int64 xrtFileGetMTime(const char* path)
{
	xfile f; xfileinfo info;
	if (!path) return 0;
	f = xrtFileOpen(path, &((xfileoptions){.Flags = XFILE_READ}));
	if (!f) return 0;
	if (xrtFileStat(f, &info)) { xrtClose(f); return (int64)info.Modified; }
	xrtClose(f);
	return 0;
}
'''

def convert(path, extra_rules=(), append_stub=False):
    src = io.open(path, encoding='utf-8').read()
    out = src
    for pat, rep in list(RULES) + list(extra_rules):
        out = re.sub(pat, rep, out)
    for form in ('<xs_plugin.h>', '"xs_plugin.h"'):
        out = out.replace('#include ' + form,
                          '#include ' + form + '\n#include "value_util.h"\n#include "util.h"', 1)
        if '#include "util.h"' in out:
            break
    if append_stub:
        anchor = out.index('#include "util.h"') + len('#include "util.h"')
        nl = out.index('\n', anchor)
        out = out[:nl] + '\n' + MULTIPART_STUB + out[nl:]
    io.open(path, 'w', encoding='utf-8', newline='').write(out)

def main():
    base = os.path.join(ROOT, 'tests/plugins')
    convert(os.path.join(base, 'guestbook_v3/main.c'))
    convert(os.path.join(base, 'filemanager/main.c'), append_stub=True)
    convert(os.path.join(base, 'xlogserver/main.c'), extra_rules=DICT_RULES)
    print('plugins converted')

if __name__ == '__main__':
    main()
