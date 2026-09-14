# -*- coding: utf-8 -*-
"""应用侧 v1 方言 → 原生 xrt 方言 机械变换（方案 A，一次性工具）。

覆盖 modules/ route_http/ main.c 的可正则化模式；语义点（take 双义、
dict 回调、目录扫描、纪元、varargs path join）由 --manual 报告清单，
配合人工补改。运行后必须过括号平衡审计 + 编译零警告 + smoke 门禁。
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (pattern, replacement) 顺序敏感：长的在前，避免前缀吞并
RULES = [
    # ---- 值创建/引用 ----
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
    (r'\bxvoArraySwap\(', 'xrtValueArraySwap('),
    # ---- 标量取值（值本体） ----
    (r'\bxvoGetText\(', 'ValueTextOf('),
    (r'\bxvoGetInt\(', 'ValueIntOf('),
    (r'\bxvoGetBool\(', 'ValueBoolOf('),
    (r'\bxvoGetFloat\(', 'ValueFloatOf('),
    (r'\bxvoGetTime\(', 'ValueTimeOf('),
    # ---- 表 Get（obj, key, len） ----
    (r'\bxvoTableGetValue\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueGet(\1, \2)'),
    (r'\bxvoTableGetText\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueText(\1, \2)'),
    (r'\bxvoTableGetInt\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueInt(\1, \2)'),
    (r'\bxvoTableGetBool\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueBool(\1, \2)'),
    (r'\bxvoTableItemType\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'xrtValueType(ValueGet(\1, \2))'),
    (r'\bxvoTableExists\(([^,()]+),\s*([^,()]+),\s*[^,()]+\)', r'ValueHas(\1, \2)'),
    # ---- 表 Set（take 双义展开；值组贪婪到行尾括号以容纳嵌套调用） ----
    (r'\bxvoTableSetValue\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*TRUE\)', r'ValueSetOwn(\1, \2, \3)'),
    (r'\bxvoTableSetValue\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*FALSE\)', r'ValueSetRef(\1, \2, \3)'),
    (r'\bxvoTableSetText\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*0,\s*TRUE\)', r'ValueSetOwnedText(\1, \2, \3)'),
    (r'\bxvoTableSetText\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*(.+),\s*0,\s*FALSE\)', r'ValueSetText(\1, \2, \3)'),
    (r'\bxvoTableSetInt\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*((?:[^()]|\([^()]*\))+)\)', r'ValueSetInt(\1, \2, \3)'),
    (r'\bxvoTableSetBool\(([^,()]+),\s*([^,()]+),\s*[^,()]+,\s*((?:[^()]|\([^()]*\))+)\)', r'ValueSetBool(\1, \2, \3)'),
    # ---- 数组 ----
    (r'\bxvoArrayAppendValue\(([^,()]+),\s*(.+),\s*TRUE\)', r'ValueArrayOwn(\1, \2)'),
    (r'\bxvoArrayAppendValue\(([^,()]+),\s*(.+),\s*FALSE\)', r'ValueArrayRef(\1, \2)'),
    (r'\bxvoArrayAppendInt\(([^,()]+),\s*(.+)\)', r'xrtValueArrayAppendNew(\1, xrtValueInt(\2))'),
    (r'\bxvoArrayGetValue\(', 'xrtValueArrayGet('),
    (r'\bxvoArrayGetText\(', 'ValueArrayText('),
    (r'\bxvoArrayGetInt\(', 'ValueArrayInt('),
    (r'\bxvoArrayItemCount\(', 'ValueCount('),
    (r'\bxvoTableItemCount\(', 'ValueCount('),
    (r'\bxvoListItemCount\(', 'ValueCount('),
    # ---- 列表（IntMap） ----
    (r'\bxvoListGetValue\(', 'ValueMapGet('),
    (r'\bxvoListGetInt\(', 'ValueMapInt('),
    (r'\bxvoListGetBool\(', 'ValueMapBool('),
    (r'\bxvoListSetValue\(([^,()]+),\s*([^,()]+),\s*(.+?),\s*TRUE\)', r'ValueMapOwn(\1, \2, \3)'),
    (r'\bxvoListSetValue\(([^,()]+),\s*([^,()]+),\s*(.+?),\s*FALSE\)', r'ValueMapRef(\1, \2, \3)'),
    (r'\bxvoListSetInt\(', 'ValueMapSetInt('),
    (r'\bxvoListSetBool\(', 'ValueMapSetBool('),
    # ---- JSON ----
    (r'\bxrtStringifyJSON\(', 'xrtJsonStringify('),
    (r'\bxrtParseJSON\(', 'JsonParseN('),
    # ---- 时间 ----
    (r'\bXA_Now\(\)', 'xrtNow()'),
    (r'\bXA_TimeToStr\((.+?),\s*XRT_TIME_FORMAT_DATETIME\)', r'TimeText(\1, TIME_TEXT_DATETIME)'),
    (r'\bXA_TimeToStr\((.+?),\s*XRT_TIME_FORMAT_DATE\)', r'TimeText(\1, TIME_TEXT_DATE)'),
    (r'\bXA_TimeToStr\((.+?),\s*XRT_TIME_FORMAT_TIME\)', r'TimeText(\1, TIME_TEXT_CLOCK)'),
    # ---- 字符串/杂项 ----
    (r'\bxrtCopyStr\(((?:[^()]|\([^()]*\))+),\s*0\)', r'xrtStrDup(\1)'),
    (r'\bxrtStrToI64\(', 'Util_ParseI64('),
    (r'\bxrtMakeXIDS\(', 'Util_Token('),
    (r'\bXA_HexEncode\(', 'Util_HexUpper('),
    (r'\bxrtPathGetExt\(([^,()]+),\s*[^,()]+\)', r'Util_ExtNoDot(\1)'),
    (r'\bxrtPathGetExt\(([^,()]+)\)', r'Util_ExtNoDot(\1)'),
    (r'\bxrtPathGetName\(([^,()]+),\s*[^,()]+\)', r'xrtPathStem(\1)'),
    (r'\bXA_FileReadAll\(([^,()]+),\s*[^,()]+,\s*([^,()]+)\)', r'xrtFileReadAll(\1, \2)'),
    # ---- dict → xrtMap ----
    (r'\bXA_DictGet\(([^,()]+),\s*([^,()]+),\s*strlen\(\2\)\)', r'xrtMapGet(\1, xrtStrView(\2))'),
    (r'\bXA_DictSet\(([^,()]+),\s*([^,()]+),\s*strlen\(\2\),\s*([^,()]+)\)', r'xrtMapGetOrAdd(\1, xrtStrView(\2), \3)'),
    (r'\bXA_DictRemove\(([^,()]+),\s*([^,()]+),\s*strlen\(\2\)\)', r'xrtMapRemove(\1, xrtStrView(\2))'),
    (r'\bxrtDictGetPtr\(', 'MANUAL_DICT_GETPTR('),
    # ---- 类型常量 ----
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
    (r'\bxdict\b', 'xmap*'),
    # ---- 布尔字面量原生 ----
    (r'\bTRUE\b', 'true'),
    (r'\bFALSE\b', 'false'),
]

MANUAL_MARKERS = [
    'xvoCreateText', 'xvoArrayAppendText', 'xvoTableSetText', 'xvoTableSetValue',
    'xvoTableGet', 'xvoTableSet', 'xvoList', 'xvoArray', 'xvoCreate', 'xvoGet',
    'xvoUnref', 'xvoAddRef', 'xvoCopy', 'xvoType',
    'XA_DictWalk', 'XA_ValueWalk', 'Dict_Key', 'XA_DictProc', 'XA_DictGet',
    'XA_DictSet', 'XA_DictRemove', 'xrtDirScan', 'xA_DirProc',
    'XA_PathJoin', 'xrtStrComp', 'xrtI64ToStr', 'XA_StrToTime', 'MANUAL_DICT',
    'xrtDictCreate', 'xrtDictDestroy', 'xrtDictSetPtr', 'xrtDictWalk',
    'XRT_TIME_FORMAT', 'XRT_OBJMODE', 'XRT_CP_', 'xvoPrintValue', 'XA_Key(',
    'xrtParseJSON_File', 'xrtStringifyJSON_File', 'xrtBufferInit', 'xrtBufferAppend',
    'XA_Now', 'XA_TimeToStr', 'xrtCopyStr', 'xrtParseJSON', 'xrtStringifyJSON',
    'XVO_DT_', 'xdict',
]

def convert_file(path):
    with io.open(path, encoding='utf-8') as f:
        src = f.read()
    out = src
    for pat, rep in RULES:
        out = re.sub(pat, rep, out)
    if out != src:
        with io.open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(out)
    return out != src

def main():
    manual_report = []
    files = [os.path.join(ROOT, 'main.c')]
    for sub in ('modules', 'route_http'):
        d = os.path.join(ROOT, sub)
        files += [os.path.join(d, n) for n in sorted(os.listdir(d)) if n.endswith('.h')]
    files = [f for f in files if 'compat_value' not in f and 'compat_util' not in f]
    changed = 0
    for path in files:
        if convert_file(path):
            changed += 1
    for path in files:
        with io.open(path, encoding='utf-8') as f:
            text = f.read()
        hits = [m for m in MANUAL_MARKERS if m in text]
        if hits:
            manual_report.append((os.path.relpath(path, ROOT), hits))
    print('converted files: %d' % changed)
    for rel, hits in manual_report:
        print('MANUAL %s: %s' % (rel, ', '.join(hits)))

if __name__ == '__main__':
    main()
