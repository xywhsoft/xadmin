# -*- coding: utf-8 -*-
"""SDK/注入表防倒退门禁（方案 A 落地后的不变量）：
1. 注入表条目要么是 SDK 声明的契约函数，要么是 xsCreateTCC 预置符号（不重复注册）；
2. plugin_sdk/ 与 modules/ 的共享头（value_util.h / util.h）逐字节一致，防双源漂移；
3. SDK 与共享头中不得再出现 v1 方言符号（xvo*/XA_*/XVO_DT_/XHTTPD_ 等）。
用法: python tests/audit_sdk.py   （退出码非 0 即门禁失败）"""
import filecmp
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
fail = []

# ---- 1) 注入表 vs SDK 声明 vs 预置重复 ----
host = open(os.path.join(ROOT, 'modules/plugin_host.h'), encoding='utf-8').read()
sdk = open(os.path.join(ROOT, 'plugin_sdk/xs_plugin.h'), encoding='utf-8').read()
injected = dict(re.findall(r'\{"(\w+)",\s+\(const void\*\)(\w+)\}', host))
sdk_funcs = set(re.findall(r'\b(\w+)\s*\(', sdk))
contract = {n for n in injected if n.startswith('XAdmin_') or n in
            ('HttpReplyFormat', 'LoadPage', 'xsHttpReplyAuto', 'xsHttpReplyFormat',
             'xsReqMethodID', 'xsReqQueryValue', 'XAdmin_PluginReqHeader',
             'XAdmin_ReqBody', 'XAdmin_ReqBodyLen')}
undeclared = sorted(n for n in contract if n not in sdk_funcs)
if undeclared:
    fail.append('注入表函数未在 SDK 声明: %s' % undeclared)

# 预置 xrt 符号（xsCreateTCC 注入）不应出现在应用注入表
xrt_decl = open(r'D:\GIT\xserver\lib\xrt_decl.h', encoding='utf-8', errors='replace').read()
preset = set(re.findall(r'XRT_API [\w\s*]+?\**\s*(\w+)\s*\(', xrt_decl))
dupes = sorted(n for n in injected if n in preset)
if dupes:
    fail.append('注入表与 xsCreateTCC 预置重复: %s' % dupes)

# ---- 2) 共享头漂移 ----
for name in ('value_util.h', 'util.h'):
    a, b = (os.path.join(ROOT, 'modules', name), os.path.join(ROOT, 'plugin_sdk', name))
    if not os.path.exists(b):
        fail.append('plugin_sdk/%s 缺失（需与 modules/ 同源分发）' % name)
    elif not filecmp.cmp(a, b, shallow=False):
        fail.append('modules/%s 与 plugin_sdk/%s 内容漂移' % (name, name))

# ---- 3) v1 方言残留 ----
ban = re.compile(r'\b(xvo\w+|XVO_DT_\w+|XHTTPD_\w+|XA_Now|XA_TimeToStr|XA_Dict\w+|Dict_Key|xdict|'
                 r'xrtCopyStr|xrtMakeXIDS|xrtStrToI64|xrtPathGetExt|xrtStringifyJSON|xrtParseJSON\w*)\b')
for rel in ('plugin_sdk/xs_plugin.h', 'plugin_sdk/value_util.h', 'plugin_sdk/util.h'):
    hits = sorted(set(ban.findall(open(os.path.join(ROOT, rel), encoding='utf-8').read())))
    if hits:
        fail.append('%s 含 v1 方言符号: %s' % (rel, hits))

if fail:
    print('AUDIT FAIL')
    for f in fail:
        print(' -', f)
    sys.exit(1)
print('AUDIT PASS: 注入项=%d（契约 %d），共享头一致，方言残留清零' %
      (len(injected), len(contract)))
