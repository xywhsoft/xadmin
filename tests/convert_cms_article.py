# -*- coding: utf-8 -*-
"""cms.article 存量生成物 v1 → v3 方言变换（阶段 3）。
复用 convert_content_tpl 的规则集（标准 + 内容补丁 + 行修 + 兼容前导），
作用于已代入占位符的 generated/main.c。"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
import convert_content_tpl as CCT

SRC = os.path.join(ROOT, 'plugin/cms.article/generated/main.c')
DST = SRC


def convert():
    out = io.open(SRC, encoding='utf-8').read()
    out = out.replace('#include "xs_plugin.h"', CCT.PREAMBLE, 1)
    for pat, rep in CCT.RULES:
        out = re.sub(pat, rep, out)
    for pat, rep in CCT.CONTENT_RULES:
        out = re.sub(pat, rep, out, flags=re.M)
    for old, new in CCT.LINE_FIXES:
        if old in out:
            out = out.replace(old, new)
    for old, new in CCT.MULTILINE_FIXES:
        if old in out:
            out = out.replace(old, new)
    leftovers = re.findall(r'\b(xvo\w+|XVO_\w+|xrtCopyStr|XHTTPD_\w+|XA_Now|xrtStringifyJSON|xrtParseJSON|xrtTimeToStr|XBUF_\w+|xrtPathJoin\(2,)\b', out)
    if leftovers:
        from collections import Counter
        print('[error] leftovers:', Counter(leftovers).most_common(20))
        return 1
    io.open(DST, 'w', encoding='utf-8', newline='').write(out)
    print('converted cms.article ->', len(out), 'bytes')
    return 0


if __name__ == '__main__':
    sys.exit(convert())
