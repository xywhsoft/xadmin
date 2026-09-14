# -*- coding: utf-8 -*-
"""managed_main.c.tpl v1 → v3 方言一次性变换（内容系统阶段 2）。
复用 native_plugin_convert 规则集 + 内容模板补丁：
- (int)/(uint32)strlen 键的 Get/Set 族（变量键，标准规则不覆盖）
- 深嵌套字面量键 Set 的逐行精确替换
- xvalue 按值声明 → xvalue*
- 兼容前导：CopyStrN/ArraySwap/ArrayInsertValue/FileGetSize/FileGetMTime/FilePutAll
输出到 content/templates/managed_main.c.tpl，随后以编译门禁验证。
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
from native_plugin_convert import RULES

SRC = os.path.join(ROOT, 'dev/v1/hosts/xadmin/data/content/templates/managed_main.c.tpl')
DST = os.path.join(ROOT, 'content/templates/managed_main.c.tpl')

# ---- 内容模板补丁规则（在标准规则之后应用） ----

CONTENT_RULES = [
    # strlen 变量键 Get 族
    (r'\bxvoTableGetValue\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\)\)', r'ValueGet(\1, \2)'),
    (r'\bxvoTableGetText\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\)\)', r'ValueText(\1, \2)'),
    (r'\bxvoTableGetInt\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\)\)', r'ValueInt(\1, \2)'),
    (r'\bxvoTableGetBool\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\)\)', r'ValueBool(\1, \2)'),
    # strlen 变量键 SetOwn/SetRef（值单层括号）
    (r'\bxvoTableSetValue\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+),\s*true\)', r'ValueSetOwn(\1, \2, \3)'),
    (r'\bxvoTableSetValue\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+),\s*false\)', r'ValueSetRef(\1, \2, \3)'),
    # strlen 变量键单行 SetInt/SetFloat/SetBool/SetNull（行尾 ); 贪婪到末括号）
    (r'^(\s*)xvoTableSetInt\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+)\);$', r'\1ValueSetInt(\2, \3, \4);'),
    (r'^(\s*)xvoTableSetFloat\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+)\);$', r'\1ValueSetFloat(\2, \3, \4);'),
    (r'^(\s*)xvoTableSetBool\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+)\);$', r'\1ValueSetBool(\2, \3, \4);'),
    (r'xvoTableSetNull\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\)\)', r'ValueSetOwn(\1, \2, xrtValueNull())'),
    # strlen 变量键 SetText（, 0, true/false 结尾）
    (r'xvoTableSetText\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+),\s*0,\s*true\)', r'ValueSetOwnedText(\1, \2, \3)'),
    (r'xvoTableSetText\(([^,()]+),\s*([^,()]+),\s*\((?:int|uint32)\)strlen\([^()]*\),\s*(.+),\s*0,\s*false\)', r'ValueSetText(\1, \2, \3)'),
    # xvoCreateText(s, 0, false) → 字符串值
    (r'\bxvoCreateText\((.+?),\s*0,\s*false\)', r'xrtValueString(xrtStrView(\1))'),
    # xrtCopyStr 深嵌套残留（标准规则的参数正则两层括号失效）
    (r'\bxrtCopyStr\((.+),\s*0\)', r'xrtStrDup(\1)'),
    (r'\bxrtCopyStr\((str)\((.+)\),\s*240\)', r'Managed_CopyStrN(\2, 240)'),
    (r'\bxrtCopyStr\((sText),\s*sText\s*\?\s*strlen\(\(const char\*\)sText\)\s*:\s*0\)', r'Managed_CopyStrN(sText, sText ? strlen((const char*)sText) : 0)'),
    # xvalue 按值声明 → 指针（不碰已有星号）
    (r'\bxvalue\s+(?![*])((?:[a-zA-Z_][a-zA-Z_0-9]*\b))', r'xvalue* \1'),
    # v1 计数式路径拼接 → v3 两参
    (r'\bxrtPathJoin\(2,\s*([^,]+),\s*([^;]+?)\)', r'xrtPathJoin(\1, \2)'),
    (r'\bxsReqPath\(', 'XAdmin_ReqPath('),
    (r'\bxsReqHeader\(', 'XAdmin_PluginReqHeader('),
    (r'\bxsReqRemote\(', 'XAdmin_ReqRemote('),
    (r'xrtBufferAppend\(&(\w+), (\s*)(.+), (\w+)i?Len?, XBUF_(BINARY|ANSI\)?)\)',
     r'xrtBufferAppend(\1, (xbytesview){(cbytes)\2, strlen(\2)})'),
    # v1 时间/缓冲/字段名残留
    (r'\bxrtTimeToStr\((.+?),\s*XRT_TIME_FORMAT_DATETIME\)', r'TimeText(\1, TIME_TEXT_DATETIME)'),
    (r'\bxrtBufferInit\((&\w+),\s*0\)', r'xrtBufferInit(\1)'),
    (r'\bxbuffer_struct\b', 'xbuffer'),
    (r'span\[0\]\.iBegin', 'span[0].Begin'),
    (r'span\[0\]\.iEnd', 'span[0].End'),
]

# 深嵌套字面量键的逐行精确替换（变换后仍残留的固定文本）
LINE_FIXES = [
    ('xvoTableSetInt(tblItem, "likeCount", 9, (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int64(stmt, 0) : 0);',
     'ValueSetInt(tblItem, "likeCount", (sqlite3_step(stmt) == SQLITE_ROW) ? (int64)sqlite3_column_int64(stmt, 0) : 0);'),
    ('xvoTableSetText(tblPayload, sSlugField, (int)strlen(sSlugField), (str)sNewSlug, 0, false);',
     'ValueSetText(tblPayload, sSlugField, sNewSlug);'),
    ('xvoTableSetBool(tblAccessOut, "payRequired", 11, sMode && (strcmp((const char*)sMode, "paid") == 0));',
     'ValueSetBool(tblAccessOut, "payRequired", sMode && (strcmp((const char*)sMode, "paid") == 0));'),
    ('xvoTableSetBool(tblAccessOut, "orderValidated", 14, sMode && (strcmp((const char*)sMode, "paid") == 0) && bAllowed);',
     'ValueSetBool(tblAccessOut, "orderValidated", sMode && (strcmp((const char*)sMode, "paid") == 0) && bAllowed);'),
    ('xvoTableSetText(tblItem, "searchSnippet", 13, sSnippetText ? sSnippetText : (str)"", 0, sSnippetText ? true : false);',
     'if (sSnippetText) ValueSetOwnedText(tblItem, "searchSnippet", sSnippetText); else ValueSetText(tblItem, "searchSnippet", "");'),
    ('xvoTableSetFloat(tblItem, "priority", 8, 0.8);', 'ValueSetFloat(tblItem, "priority", 0.8);'),
    ('xvoTableSetText(tblRet, "event", 5, (str)"form.notification.replay", 24, false);',
     'ValueSetText(tblRet, "event", "form.notification.replay");'),
    ('xvoTableSetText(tblRet, "warning", 7, "import preview job was not saved", -1, false);',
     'ValueSetText(tblRet, "warning", "import preview job was not saved");'),
    ('xvoTableSetText(tblRet, "warning", 7, "import job was not saved", -1, false);',
     'ValueSetText(tblRet, "warning", "import job was not saved");'),
    ('xvoTableSetText(tblRet, "warning", 7, "export job was not saved", -1, false);',
     'ValueSetText(tblRet, "warning", "export job was not saved");'),
    ('xvoTableSetInt(tblRow, "beforeLength", 12, sBefore ? (int)strlen((const char*)sBefore) : 0);',
     'ValueSetInt(tblRow, "beforeLength", sBefore ? (int64)strlen((const char*)sBefore) : 0);'),
    ('xvoTableSetInt(tblRow, "afterLength", 11, sAfter ? (int)strlen((const char*)sAfter) : 0);',
     'ValueSetInt(tblRow, "afterLength", sAfter ? (int64)strlen((const char*)sAfter) : 0);'),
    ('xvoTableSetInt(tblRow, "beforeLineCount", 15, Managed_RevisionLineCount(sBefore ? (const char*)sBefore : ""));',
     'ValueSetInt(tblRow, "beforeLineCount", Managed_RevisionLineCount(sBefore ? (const char*)sBefore : ""));'),
    ('xvoTableSetInt(tblRow, "afterLineCount", 14, Managed_RevisionLineCount(sAfter ? (const char*)sAfter : ""));',
     'ValueSetInt(tblRow, "afterLineCount", Managed_RevisionLineCount(sAfter ? (const char*)sAfter : ""));'),
    ('xvoTableSetFloat(tblRow, "beforeNumber", 12, fBefore);', 'ValueSetFloat(tblRow, "beforeNumber", fBefore);'),
    ('xvoTableSetFloat(tblRow, "afterNumber", 11, fAfter);', 'ValueSetFloat(tblRow, "afterNumber", fAfter);'),
    ('xvoTableSetFloat(tblRow, "numberDelta", 11, fAfter - fBefore);', 'ValueSetFloat(tblRow, "numberDelta", fAfter - fBefore);'),
    ('(tblBeforeData && (xrtValueType(tblBeforeData) == XVALUE_OBJECT)) ? xvoTableGetValue(tblBeforeData, sName, (uint32)strlen(sName)) : NULL,',
     '(tblBeforeData && (xrtValueType(tblBeforeData) == XVALUE_OBJECT)) ? ValueGet(tblBeforeData, sName) : NULL,'),
    ('(tblAfterData && (xrtValueType(tblAfterData) == XVALUE_OBJECT)) ? xvoTableGetValue(tblAfterData, sName, (uint32)strlen(sName)) : NULL);',
     '(tblAfterData && (xrtValueType(tblAfterData) == XVALUE_OBJECT)) ? ValueGet(tblAfterData, sName) : NULL);'),
    ('arrPaid = xvoTableGetValue(objSession, sSessionKey ? (const char*)sSessionKey : "paidContentIds", sSessionKey ? (int)strlen((const char*)sSessionKey) : (int)strlen("paidContentIds"));',
     'arrPaid = ValueGet(objSession, sSessionKey ? (const char*)sSessionKey : "paidContentIds");'),
    ('xvoArraySwap(arrItems, i, j);', 'Managed_ArraySwap(arrItems, i, j);'),
    ('xvoArrayInsertValue(arrList, i, tblItem, true);', 'xrtValueArrayInsertTake(arrList, i, &tblItem);'),
    ('str ServerHashPassword(str user, str salt, str clientHash);', ''),
    ('xrtBufferAppend(&tBuf, (ptr)sEsc, (uint32)strlen(sEsc), XBUF_BINARY);',
     'xrtBufferAppend(&tBuf, (xbytesview){(cbytes)sEsc, strlen(sEsc)});'),
    ('xrtBufferAppend(&tBuf, &ch, 1, XBUF_BINARY);',
     'xrtBufferAppendByte(&tBuf, (uint8)ch);'),
    ('xrtBufferAppend(&tBuf, (ptr)p, 1, XBUF_BINARY);',
     'xrtBufferAppend(&tBuf, (xbytesview){(cbytes)p, 1});'),
    ('return (str)tBuf.Buffer;', 'return (str)tBuf.Data;'),
    # slug 动态路由：v1 正则 pattern → v3 pattern 方言（命名捕获，免转义）
    ('str sSlugRoutePrefixRegex = Managed_RegexEscapeLiteralDup(sSlugRoutePrefix ? (const char*)sSlugRoutePrefix : "/{{PLUGIN_XID}}");' + chr(10) + chr(9)*4 + 'str sSlugRoutePattern = xrtFormat("^%s/([^/]+)$", sSlugRoutePrefixRegex ? (const char*)sSlugRoutePrefixRegex : "/{{PLUGIN_XID}}");',
     'str sSlugRoutePattern = xrtFormat("%s/{slug}", sSlugRoutePrefix ? (const char*)sSlugRoutePrefix : "/{{PLUGIN_XID}}");'),
    ('str sSlugRoutePrefixRegex = Managed_RegexEscapeLiteralDup(sSlugRoutePrefix ? (const char*)sSlugRoutePrefix : "/cms.article");' + chr(10) + chr(9)*4 + 'str sSlugRoutePattern = xrtFormat("^%s/([^/]+)$", sSlugRoutePrefixRegex ? (const char*)sSlugRoutePrefixRegex : "/cms.article");',
     'str sSlugRoutePattern = xrtFormat("%s/{slug}", sSlugRoutePrefix ? (const char*)sSlugRoutePrefix : "/cms.article");'),
    (chr(9)*5 + 'if ( sSlugRoutePrefixRegex ) xrtFree(sSlugRoutePrefixRegex);' + chr(10), chr(10)),
    (chr(9)*5 + 'if ( sSlugRoutePrefixRegex ) xrtFree(sSlugRoutePrefixRegex);' + chr(10), chr(10)),
    # redirect 动态路由：v1 正则 → v3 尾段捕获
    ('str sRedirectRoutePrefixRegex = Managed_RegexEscapeLiteralDup(sRedirectRoutePrefix ? (const char*)sRedirectRoutePrefix : "/{{PLUGIN_XID}}/r");' + chr(10) + chr(9)*4 + 'str sRedirectRoutePattern = xrtFormat("^%s/[^?#]+$", sRedirectRoutePrefixRegex ? (const char*)sRedirectRoutePrefixRegex : "/{{PLUGIN_XID}}/r");',
     'str sRedirectRoutePattern = xrtFormat("%s/{*target}", sRedirectRoutePrefix ? (const char*)sRedirectRoutePrefix : "/{{PLUGIN_XID}}/r");'),
    (chr(9)*5 + 'if ( sRedirectRoutePrefixRegex ) xrtFree(sRedirectRoutePrefixRegex);' + chr(10), chr(10)),
    (chr(9)*4 + 'if ( sRedirectRoutePrefixRegex ) xrtFree(sRedirectRoutePrefixRegex);' + chr(10), chr(10)),
    (chr(9)*5 + 'if ( sSlugRoutePrefixRegex ) xrtFree(sSlugRoutePrefixRegex);' + chr(10), chr(10)),
    # redirect 动态路由：v1 正则 → v3 尾段捕获
    ('str sRedirectRoutePrefixRegex = Managed_RegexEscapeLiteralDup(sRedirectRoutePrefix ? (const char*)sRedirectRoutePrefix : "/cms.article/r");' + chr(10) + chr(9)*4 + 'str sRedirectRoutePattern = xrtFormat("^%s/[^?#]+$", sRedirectRoutePrefixRegex ? (const char*)sRedirectRoutePrefixRegex : "/cms.article/r");',
     'str sRedirectRoutePattern = xrtFormat("%s/{*target}", sRedirectRoutePrefix ? (const char*)sRedirectRoutePrefix : "/cms.article/r");'),
    (chr(9)*5 + 'if ( sRedirectRoutePrefixRegex ) xrtFree(sRedirectRoutePrefixRegex);' + chr(10), chr(10)),
    (chr(9)*4 + 'if ( sRedirectRoutePrefixRegex ) xrtFree(sRedirectRoutePrefixRegex);' + chr(10), chr(10)),
    (chr(9)*4 + 'if ( sSlugRoutePrefixRegex ) xrtFree(sSlugRoutePrefixRegex);' + chr(10), chr(10)),

    ('if ( tBuf.Length == 0 ) {', 'if ( tBuf.Size == 0 ) {'),
    ('xrtBufferAppend(&tBuf, "item", 4, XBUF_BINARY);',
     'xrtBufferAppend(&tBuf, (xbytesview){(cbytes)"item", 4});'),
    ('xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY);',
     'xrtBufferAppendByte(&tBuf, (uint8)chZero);'),
    ('int Managed_ScanProviderPluginProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)',
     'int Managed_ScanProviderPluginProc(const char* sPath, size_t iSize, bool bDir, void* Param)'),
    ('	(void)pData;', ''),
    ('arrPaid = xvoTableGetValue(objSession, sSessionKey ? (const char*)sSessionKey : "paidContentIds", sSessionKey ? (int)strlen((const char*)sSessionKey) : 14);',
     'arrPaid = ValueGet(objSession, sSessionKey ? (const char*)sSessionKey : "paidContentIds");'),
    ('xrtCopyStr((str)sHtml, (uint32)iHtmlSize)', 'Managed_CopyStrN(sHtml, (size_t)iHtmlSize)'),
    ('xrtCopyStr((str)(sText + iStart), 240)', 'Managed_CopyStrN(sText + iStart, 240)'),
    ('xrtCopyStr(sText, sText ? strlen((const char*)sText) : 0)', 'Managed_CopyStrN(sText, sText ? strlen((const char*)sText) : 0)'),
    ('xvoTableSetInt(tblData, sName, (int)strlen(sName), (int64)ValueFloatOf(objValue));',
     'ValueSetInt(tblData, sName, (int64)ValueFloatOf(objValue));'),
    ('xvoTableSetInt(tblData, sName, (int)strlen(sName), atoll(ValueTextOf(objValue)));',
     'ValueSetInt(tblData, sName, Util_ParseI64(ValueTextOf(objValue)));'),
    ('xvoTableSetFloat(tblData, sName, (int)strlen(sName), (double)ValueIntOf(objValue));',
     'ValueSetFloat(tblData, sName, (double)ValueIntOf(objValue));'),
    ('xvoTableSetFloat(tblData, sName, (int)strlen(sName), strtod(ValueTextOf(objValue), NULL));',
     'ValueSetFloat(tblData, sName, strtod(ValueTextOf(objValue), NULL));'),
    ('xvoTableSetBool(tblData, sName, (int)strlen(sName), ValueIntOf(objValue) != 0);',
     'ValueSetBool(tblData, sName, ValueIntOf(objValue) != 0);'),
    ('xvoTableSetBool(tblData, sName, (int)strlen(sName), ValueFloatOf(objValue) != 0.0);',
     'ValueSetBool(tblData, sName, ValueFloatOf(objValue) != 0.0);'),
]

# 多行 xvoTableSetBool( … ) 残留（变换后仍跨行）
MULTILINE_FIXES = [
    ('''xvoTableSetBool(
				tblData,
				sName,
				(int)strlen(sName),
				Managed_TextEqualsIgnoreCase(sText, "true") || (strcmp(sText, "1") == 0));''',
     '''ValueSetBool(tblData, sName, Managed_TextEqualsIgnoreCase(sText, "true") || (strcmp(sText, "1") == 0));'''),
]

# 兼容前导：v1 侧文件/数组 API 的 v3 等价封装
PREAMBLE = '''#include "xs_plugin.h"
#include "value_util.h"
#include "util.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* v1 兼容封装（v3 原生等价） */
static str Managed_CopyStrN(const char* s, size_t n)
{
	str out;
	if (!s || !n) return xrtStrDup("");
	out = (str)xrtMalloc(n + 1);
	if (!out) return xrtStrDup("");
	memcpy(out, s, n);
	out[n] = '\\0';
	return out;
}
static void Managed_ArraySwap(xvalue* arr, uint32 i, uint32 j)
{
	xvalue* a = xrtValueDeepClone(xrtValueArrayGet(arr, i));
	xvalue* b = xrtValueDeepClone(xrtValueArrayGet(arr, j));
	if (!a || !b) { xrtValueRelease(a); xrtValueRelease(b); return; }
	xrtValueArraySetTake(arr, i, &b);
	xrtValueArraySetTake(arr, j, &a);
}
static int64 xrtFileGetSize(const char* path)
{
	xfileinfo info;
	if (!path || !xrtPathStat(path, true, &info) || info.Type != XFILE_TYPE_FILE) return 0;
	return (int64)info.Size;
}
static int64 xrtFileGetMTime(const char* path)
{
	xfileinfo info;
	if (!path || !xrtPathStat(path, true, &info) || info.Type != XFILE_TYPE_FILE) return 0;
	return (int64)info.Changed;
}
#define xrtFileGetAll(p, n) ((char*)xrtFileReadAll(p, n))
static bool xrtFilePutAll(const char* path, const void* data, size_t size)
{
	return xrtFileWriteAtomic(path, (xbytesview){(cbytes)data, size});
}
#define xrtFileExists(p) xrtPathExists(p)
#define xrtDirScan(d, r, proc, ctx) DirScan(d, r, proc, ctx)
#define xrtPathGetDir(p, n) xrtPathParent(p)
#define xrtFileGetChangeTime(p) xrtFileGetMTime(p)
static str xrtPathGetNameExt(const char* path, int unused)
{
	const char* name;
	(void)unused;
	if (!path) return xrtStrDup("");
	name = path + strlen(path);
	while (name > path && name[-1] != '/' && name[-1] != '\\\\') name--;
	return xrtStrDup(name);
}
static str xrtReplace(const char* text, size_t textLen, const char* find, size_t findLen,
	const char* repl, size_t replLen, void* unused)
{
	xbuffer* buf; str out; const char* cur; const char* hit; size_t flen; size_t rlen;
	(void)unused; (void)textLen; (void)findLen; (void)replLen;
	if (!text || !find || !find[0]) return xrtStrDup(text ? text : "");
	flen = strlen(find); rlen = repl ? strlen(repl) : 0;
	buf = xrtBufferCreate();
	if (!buf) return xrtStrDup(text);
	cur = text;
	while ((hit = strstr(cur, find)) != NULL) {
		xrtBufferAppend(buf, (xbytesview){(cbytes)cur, (size_t)(hit - cur)});
		if (rlen) xrtBufferAppend(buf, (xbytesview){(cbytes)repl, rlen});
		cur = hit + flen;
	}
	xrtBufferAppend(buf, (xbytesview){(cbytes)cur, strlen(cur)});
	out = buf->Data ? xrtStrDup((const char*)buf->Data) : xrtStrDup("");
	xrtBufferDestroy(buf);
	return out;
}
static xregex* xrtRegexCreate(const char* pattern)
{
	return pattern ? xrtRegexCompile(xrtStrView(pattern)) : NULL;
}
static void xrtRegexDestroy(xregex* regex)
{
	if (regex) xrtRegexRelease(regex);
}
static const char* xrtRegexGetErrorMsg(xregex* regex)
{
	(void)regex;
	return "";
}
/* markdown 渲染：宿主 md4c（md_html 符号经 xsCreateTCC 预置）。
 * 旗标取值与上游 md4c.h/md4c-html.h 一致；v1 契约 GFM+NOHTML+SKIP_BOM。 */
#define MD_FLAG_NOHTMLBLOCKS                0x20u
#define MD_FLAG_NOHTMLSPANS                 0x40u
#define MD_FLAG_TABLES                      0x100u
#define MD_FLAG_STRIKETHROUGH               0x200u
#define MD_FLAG_TASKLISTS                   0x800u
#define MD_FLAG_PERMISSIVEEMAILAUTOLINKS    0x8u
#define MD_FLAG_PERMISSIVEURLAUTOLINKS      0x4u
#define MD_FLAG_PERMISSIVEWWWAUTOLINKS      0x400u
#define MD_FLAG_PERMISSIVEAUTOLINKS         (MD_FLAG_PERMISSIVEEMAILAUTOLINKS | MD_FLAG_PERMISSIVEURLAUTOLINKS | MD_FLAG_PERMISSIVEWWWAUTOLINKS)
#define MD_FLAG_ADMONITIONS                 0x80000u
#define MD_FLAG_FOOTNOTES                   0x100000u
#define MD_FLAG_NOHTML                      (MD_FLAG_NOHTMLBLOCKS | MD_FLAG_NOHTMLSPANS)
#define MD_DIALECT_GITHUB                   (MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS | MD_FLAG_ADMONITIONS | MD_FLAG_FOOTNOTES)
#define MD_HTML_FLAG_SKIP_UTF8_BOM          0x0004u
typedef char MD_CHAR_;
typedef unsigned MD_SIZE_;
extern int md_html(const char* input, unsigned input_size,
	void (*process_output)(const char*, unsigned, void*),
	void* userdata, unsigned parser_flags, unsigned renderer_flags);
static void Managed_MdOutput(const char* data, unsigned size, void* userdata)
{
	xrtBufferAppend((xbuffer*)userdata, (xbytesview){(cbytes)data, size});
}
static char* xsMarkdownToHtmlEx(const char* text, unsigned dialect, unsigned flags)
{
	xbuffer* buf = xrtBufferCreate();
	char* out;
	if (!buf) return NULL;
	if (!text) text = "";
	if (md_html(text, (unsigned)strlen(text), Managed_MdOutput, buf, dialect, flags) != 0) {
		xrtBufferDestroy(buf);
		return NULL;
	}
	out = buf->Data ? xrtStrDup((const char*)buf->Data) : xrtStrDup("");
	xrtBufferDestroy(buf);
	return out;
}
static int xrtRegexCaptures(xregex* regex, const char* text, size_t len, xregexspan* spans, int max)
{
	xregexmatcher* matcher;
	xregexresult rc;
	(void)max;
	if (!regex || !text) return 0;
	matcher = xrtRegexMatcherCreate(regex);
	if (!matcher) return 0;
	rc = xrtRegexMatcherFind(matcher, xrtStrViewN(text, len), 0);
	if (rc == XREGEX_MATCH) {
		xregexcapture capture;
		memset(&capture, 0, sizeof(capture));
		if (spans && xrtRegexMatcherCapture(matcher, 0, &capture) && capture.Matched) {
			spans[0].Begin = capture.Span.Begin;
			spans[0].End = capture.Span.End;
		} else if (spans) {
			spans[0].Begin = 0;
			spans[0].End = len;
		}
		xrtRegexMatcherFree(matcher);
		return 1;
	}
	xrtRegexMatcherFree(matcher);
	return 0;
}
'''


def convert():
    src = io.open(SRC, encoding='utf-8').read()
    out = src
    # 替换原 xs_plugin.h include 为完整前导（保留其后的 CAP ifdef 段）
    out = out.replace('#include "xs_plugin.h"', PREAMBLE, 1)
    for pat, rep in RULES:
        out = re.sub(pat, rep, out)
    for pat, rep in CONTENT_RULES:
        out = re.sub(pat, rep, out, flags=re.M)
    for old, new in LINE_FIXES:
        if old not in out:
            print('[warn] line fix not found:', old[:70])
        out = out.replace(old, new)
    for old, new in MULTILINE_FIXES:
        if old not in out:
            print('[warn] multiline fix not found:', old[:70])
        out = out.replace(old, new)
    # 终检：残留 v1 标记
    leftovers = re.findall(r'\b(xvo\w+|XVO_\w+|xrtCopyStr|XHTTPD_\w+|XA_Now|xrtStringifyJSON|xrtParseJSON)\b', out)
    if leftovers:
        from collections import Counter
        print('[error] leftovers:', Counter(leftovers).most_common(20))
        return 1
    io.open(DST, 'w', encoding='utf-8', newline='').write(out)
    print('converted ->', DST, len(out), 'bytes')
    return 0


if __name__ == '__main__':
    sys.exit(convert())
