"""通用 v1 → xs3 插件机械变换（v1hello 已验证的模式泛化到任意插件）。

用法：python transform_v1.py <xid>
产物写入 tests/plugins/<xid>/，可直接被 smoke 夹具复用。
"""
import json, re, shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# v1 名称 → xs3 名称 映射（compat 层已有或直接映射）
FUNC_MAP = {
    'xrtFileGetAll': 'xrtFileReadAll',
    'xrtFilePutAll': 'xrtFileWriteAll',
    'xrtPathRandom': 'xrtPathTempNew',
    'xrtFileGetChangeTime': 'xrtFileGetMTime',
    'xrtStrToI(': 'xrtStrToI64(',
    'xrtDirDelete(': 'xrtDirDeleteAll(',
}

def transform_source(src: str) -> str:
    t = src
    # 类型系统
    t = re.sub(r'\bxvalue\b(?!\*)', 'xvalue*', t)
    t = re.sub(r'(\b\w+(?:->\w+)*)->Type\b', r'xrtValueType(\1)', t)
    t = re.sub(r'xrtPtrArrayGet_Inline\((\w+)->vArray,\s*([^)]*)\)', r'xvoArrayGetValue(\1, (\2) - 1)', t)
    t = re.sub(r'xrtPtrArrayGet_Inline\((\w+),\s*([^)]*)\)', r'xvoArrayGetValue(\1, (\2) - 1)', t)
    t = re.sub(r'xrtDictWalk\((\w+)->vTable,', r'XA_ValueWalk(\1,', t)
    t = re.sub(r'xrtListWalk\((\w+)->vList,', r'XA_ValueWalk(\1,', t)
    # 函数名映射
    for old, new in FUNC_MAP.items():
        t = re.sub(re.escape(old), new, t)
    # v1 的 xrtPathJoin(count, ...) → 新版 xrtPathJoin(a, b)
    t = re.sub(r'xrtPathJoin\(\s*(\d+)\s*,\s*', 'xrtPathJoin(', t)
    # xdict 类型（如果还有残留）
    # v1 习惯
    t = t.replace('xrtNow()', 'XA_Now()')
    t = t.replace('xvoType(', 'xrtValueType(')
    # xrtStrView 类型（v1 的 xrtstrview → 新版 xstrview）
    t = re.sub(r'\bxrtstrview\b', 'xstrview', t)
    t = re.sub(r'\bxrtbytesview\b', 'xbytesview', t)
    return t

def add_includes(t: str) -> str:
    includes = ['#include <xs_plugin.h>']
    if 'sqlite3_' in t or 'sqlite3 ' in t or 'sqlite3*' in t:
        includes.append('#include <sqlite3.h>')
    if 'strlen' in t or 'strchr' in t or 'strcmp' in t or 'memcpy' in t:
        includes.append('#include <string.h>')
    if 'snprintf' in t or 'printf' in t or 'vsnprintf' in t:
        includes.append('#include <stdio.h>')
    if 'malloc' in t or 'free(' in t:
        includes.append('#include <stdlib.h>')
    # 替换第一个 include
    t = re.sub(r'#include\s*<xs_plugin\.h>', '\n'.join(includes), t, count=1)
    return t

def main():
    if len(sys.argv) < 2:
        print('usage: transform_v1.py <xid>')
        return 1
    xid = sys.argv[1]
    src_dir = ROOT / 'plugin' / xid
    out_dir = ROOT / 'tests/plugins' / xid
    if not (src_dir / 'main.c').exists():
        print(f'not found: {src_dir}/main.c')
        return 1
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    shutil.copytree(src_dir, out_dir, dirs_exist_ok=True)

    src = (src_dir / 'main.c').read_text(encoding='utf-8', errors='surrogateescape')
    t = transform_source(src)
    t = add_includes(t)
    (out_dir / 'main.c').write_text(t, encoding='utf-8', errors='surrogateescape')

    mf = json.loads((out_dir / 'plugin.json').read_text(encoding='utf-8'))
    mf['xid'] = mf['name'] = xid
    (out_dir / 'plugin.json').write_text(json.dumps(mf, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'transformed {xid} -> {out_dir}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
