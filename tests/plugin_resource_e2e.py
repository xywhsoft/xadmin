# -*- coding: utf-8 -*-
"""插件资源体系 e2e：/plugin-static/ 静态服务 + page/template/option ABI + 编译期约定目录。

正例：MIME/ETag/版本化缓存策略、HEAD、LoadPluginPage、RenderPluginTemplate、
OptionLoad/Save（数据目录覆盖落盘）、PluginResourcePath、inc/src 约定目录头文件。
负例：未知插件、路径穿越（%2e 编码与 .. 明文）、sourcemap 门控、白名单外扩展名、
禁用后 404。回归：reload 后静态服务仍可用。
"""
import sys, time, subprocess, json, sqlite3
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18236
target = smoke.fixture(PORT)
# libraries 实链预置：manifest 注入 define + libraries/libraryDirs，System32 的
# iphlpapi.dll 拷入 lib/ 约定目录（同时覆盖 tcc_add_library_path 约定目录与显式
# libraryDirs 两条路径 + tcc_add_library 按名解析 DLL）
import os
_dlldir = target / 'plugin/hello-sdk/lib'
_dlldir.mkdir(parents=True, exist_ok=True)
try:
    import shutil as _shutil
    _shutil.copy(os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'System32', 'iphlpapi.dll'), _dlldir / 'iphlpapi.dll')
    _have_dll = True
except OSError:
    _have_dll = False
_mpath = target / 'plugin/hello-sdk/plugin.json'
_m = json.loads(_mpath.read_text(encoding='utf-8'))
_m['build']['defines'].append('HELLO_SDK_LINK_IPHLPAPI=1')
_m['build']['libraries'].append('iphlpapi')
_m['build']['libraryDirs'].append('lib')
_mpath.write_text(json.dumps(_m, ensure_ascii=False, indent=2), encoding='utf-8')
proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                        stdout=open(target / 'server.log', 'ab'), stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
ok = fail = 0
def check(name, cond, detail=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '|', detail)

try:
    for _ in range(80):
        if proc.poll() is not None:
            raise RuntimeError('server exited early')
        try:
            if smoke.request(PORT, 'GET', '/admin/login')[0] in (200, 404):
                break
        except OSError:
            pass
        time.sleep(0.3)
    st, hd, _ = smoke.request(PORT, 'POST', '/admin/login',
                              {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
    cookie = hd['Set-Cookie'].split(';')[0]

    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, cookie=cookie)
    check('enable hello-sdk', json.loads(body).get('result') is True, body[:200])

    # ---- /plugin-static/ 基础 ----
    st, hd, body = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/hello.css')
    check('static css 200', st == 200, 'status=%d' % st)
    check('static css body', b'.hello-sdk-static' in body, body[:100])
    check('static css content-type', 'text/css' in hd.get('Content-Type', ''), hd.get('Content-Type'))
    check('static etag present', 'ETag' in hd and hd['ETag'].startswith('"p-'), str(hd.get('ETag')))
    check('static last-modified present', 'Last-Modified' in hd, str(hd.get('Last-Modified')))
    check('static no-cache (unversioned)', 'no-cache' in hd.get('Cache-Control', ''), hd.get('Cache-Control'))

    # 版本化文件名 → immutable 长缓存
    st, hd, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/hello.4f3a2b1c9d8e.css')
    check('versioned static 200', st == 200, 'status=%d' % st)
    check('versioned immutable cache', 'immutable' in hd.get('Cache-Control', '') and '31536000' in hd.get('Cache-Control', ''),
          hd.get('Cache-Control'))

    # HEAD：头齐全、无正文
    st, hd, body = smoke.request(PORT, 'HEAD', '/plugin-static/hello-sdk/hello.css')
    check('static HEAD 200 empty', st == 200 and body == b'', 'status=%d len=%d' % (st, len(body)))

    # ---- 负例 ----
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/unknown-plugin/hello.css')
    check('unknown plugin 404', st == 404, 'status=%d' % st)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/no-such-file.css')
    check('missing file 404', st == 404, 'status=%d' % st)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/hello.css.map')
    check('sourcemap gated 404', st == 404, 'status=%d' % st)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/plugin.json')
    check('non-whitelist ext 404', st == 404, 'status=%d' % st)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/%2e%2e/main.c')
    check('encoded traversal 404', st == 404, 'status=%d' % st)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/..%5cplugin.json')
    check('backslash traversal 404', st == 404, 'status=%d' % st)
    st, _, _ = smoke.request(PORT, 'POST', '/plugin-static/hello-sdk/hello.css', {'x': 1})
    check('non-GET rejected 404', st == 404, 'status=%d' % st)

    # ---- page ABI ----
    st, hd, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/page')
    check('LoadPluginPage 200 html', st == 200 and b'hello-sdk page resource' in body,
          'status=%d body=%s' % (st, body[:120]))

    # ---- template ABI ----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/template')
    check('RenderPluginTemplate rendered', st == 200 and b'Hello hello-sdk from plugin template!' in body,
          'status=%d body=%s' % (st, body[:150]))

    # ---- option ABI：装载（包内默认值）+ 约定目录宏 ----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/option')
    r = json.loads(body)
    check('OptionLoad default value', r.get('value') == 'hello-option-default', body[:150])
    check('inc/src convention dirs compiled', r.get('conv') == 49, body[:150])

    # ---- option save：写入数据目录覆盖 ----
    st, _, body = smoke.request(PORT, 'POST', '/api/plugin/hello-sdk/option-save',
                                {'welcome_message': 'saved-by-e2e'})
    r = json.loads(body)
    check('OptionSave ok + reload new value', r.get('result') is True and r.get('value') == 'saved-by-e2e', body[:150])
    override = target / 'plugin_data/hello-sdk/option/runtime.json'
    check('option override file on disk', override.exists())
    if override.exists():
        data = json.loads(override.read_text(encoding='utf-8'))
        check('override file structure classList', data.get('classList', [{}])[0].get('options', [{}])[0].get('value') == 'saved-by-e2e')
    # 包内 option 文件必须保持原值（不被污染）
    pkg = json.loads((target / 'plugin/hello-sdk/option/runtime.json').read_text(encoding='utf-8'))
    check('package option untouched', pkg['classList'][0]['options'][0]['value'] == 'hello-option-default')

    # ---- resource path ABI ----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/resource-path')
    r = json.loads(body)
    check('PluginResourcePath ok', r.get('result') is True and r.get('endsWithHelloCss') is True, body[:150])

    # ---- libraries 实链（iphlpapi.dll 经 lib/ 约定目录 + tcc_add_library 解析） ----
    if _have_dll:
        st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/link-probe')
        r = json.loads(body)
        check('iphlpapi real link + call', r.get('result') is True and r.get('interfaces', 0) >= 1,
              'status=%d body=%s' % (st, body[:200]))
    else:
        print('SKIP iphlpapi real link (System32 dll not available)')

    # ---- reload 后静态与资源仍可用 ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/reload', {'name': 'hello-sdk'}, cookie=cookie)
    check('reload ok', json.loads(body).get('result') is True, body[:200])
    time.sleep(0.3)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/hello.css')
    check('static alive after reload', st == 200, 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/option')
    check('option override survives reload', json.loads(body).get('value') == 'saved-by-e2e', body[:150])

    # ---- 禁用后静态 404（v1 仅 ACTIVE 包可访问） ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/disable', {'name': 'hello-sdk'}, cookie=cookie)
    check('disable ok', json.loads(body).get('result') is True, body[:200])
    time.sleep(0.3)
    st, _, _ = smoke.request(PORT, 'GET', '/plugin-static/hello-sdk/hello.css')
    check('static 404 after disable', st == 404, 'status=%d' % st)
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
warn = [l for l in log.splitlines() if 'warning' in l.lower() or 'error' in l.lower()]
print('=' * 60)
print('compile warnings/errors:', warn if warn else 'NONE')
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
