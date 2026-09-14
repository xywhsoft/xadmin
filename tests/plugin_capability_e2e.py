# -*- coding: utf-8 -*-
"""插件能力面第二批 e2e：脚手架生成 / 动态路由 ABI / 新 auth 自动授予 /
编译错误聚合落库 / settings GET+schema+回滚 / manifest 轻量校验。"""
import sys, time, subprocess, json, sqlite3, shutil
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18245
target = smoke.fixture(PORT)

# 夹具期预置：坏 C 插件（编译错误聚合）+ xid 不匹配插件（manifest 校验）
badc = target / 'plugin/badc'
badc.mkdir(parents=True, exist_ok=True)
(badc / 'main.c').write_text('#include <xs_plugin.h>\nint broken( {\n', encoding='utf-8')
(badc / 'plugin.json').write_text(json.dumps({
    'formatVersion': 4, 'xid': 'badc', 'name': 'badc', 'title': 'Bad C', 'version': '1.0.0',
    'kind': 'singleton', 'runtime': {'compiler': 'tcc', 'language': 'c'},
    'build': {'entry': 'main.c', 'sources': ['main.c'], 'includeDirs': [], 'libraryDirs': [], 'libraries': [], 'defines': ['XADMIN_PLUGIN=1']},
    'compat': {'minHostVersion': '4.0.0', 'maxHostVersion': '5.0.0', 'abiVersion': 4},
}), encoding='utf-8')
wrong = target / 'plugin/wrongid'
wrong.mkdir(parents=True, exist_ok=True)
(wrong / 'main.c').write_text('#include <xs_plugin.h>\n', encoding='utf-8')
(wrong / 'plugin.json').write_text(json.dumps({
    'formatVersion': 4, 'xid': 'not-the-dir-name', 'name': 'wrongid', 'title': 'Wrong', 'version': '1.0.0',
    'kind': 'singleton', 'runtime': {'compiler': 'tcc', 'language': 'c'},
    'build': {'entry': 'main.c', 'sources': ['main.c'], 'includeDirs': [], 'libraryDirs': [], 'libraries': [], 'defines': ['XADMIN_PLUGIN=1']},
    'compat': {'minHostVersion': '4.0.0', 'maxHostVersion': '5.0.0', 'abiVersion': 4},
}), encoding='utf-8')

proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                        stdout=open(target / 'server.log', 'ab'), stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
ok = fail = 0
def check(name, cond, detail=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '|', detail)

def plugin_names(cookie):
    st, _, body = smoke.request(PORT, 'GET', '/admin/plugin/list', cookie=cookie)
    data = json.loads(body).get('data')
    items = data if isinstance(data, list) else data.get('list', [])
    return {it.get('name') for it in items if isinstance(it, dict)}

try:
    for _ in range(80):
        if proc.poll() is not None:
            raise RuntimeError((target / 'server.log').read_text(encoding='utf-8', errors='replace')[-1500:])
        try:
            if smoke.request(PORT, 'GET', '/admin/login')[0] in (200, 404): break
        except OSError: pass
        time.sleep(0.3)
    st, hd, _ = smoke.request(PORT, 'POST', '/admin/login',
                              {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
    cookie = hd['Set-Cookie'].split(';')[0]

    # ---- manifest 轻量校验：xid 不匹配的 wrongid 不被发现；badc 正常发现 ----
    names = plugin_names(cookie)
    check('manifest identity rejects wrongid', 'wrongid' not in names, str(sorted(n for n in names if 'wrong' in n or n == 'badc')))
    check('valid badc still discovered', 'badc' in names)

    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, cookie=cookie)
    check('enable hello-sdk', json.loads(body).get('result') is True, body[:150])

    # ---- 新 auth 自动授予：admin-echo 超管 200（此前实测 403） ----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/admin-echo', cookie=cookie)
    check('fresh auth auto-granted to role 1 (was 403)', st == 200 and json.loads(body).get('result') is True,
          'status=%d body=%s' % (st, body[:120]))
    with sqlite3.connect(target / 'db/main.db') as db:
        aid = db.execute("SELECT id FROM auth WHERE plugin_xid='hello-sdk' AND isDelete=0").fetchone()[0]
        r1 = json.loads(db.execute("SELECT authList FROM role WHERE id=1").fetchone()[0])
        check('role1 authList contains new auth', aid in r1, 'auth=%d list=%s' % (aid, r1))

    # ---- 动态路由 ABI ----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/dyn/world')
    r = json.loads(body)
    check('dynamic route pattern match', r.get('name') == 'world' and r.get('paramCount') == 1, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/dyn/abc-123')
    check('dynamic route another value', json.loads(body).get('name') == 'abc-123', body[:150])
    # reload 后动态路由重注册
    smoke.request(PORT, 'POST', '/admin/plugin/reload', {'name': 'hello-sdk'}, cookie=cookie)
    time.sleep(0.3)
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/dyn/after-reload')
    check('dynamic route re-registered after reload', st == 200 and json.loads(body).get('name') == 'after-reload',
          'status=%d body=%s' % (st, body[:120]))
    # disable 后动态路由下线
    smoke.request(PORT, 'POST', '/admin/plugin/disable', {'name': 'hello-sdk'}, cookie=cookie)
    time.sleep(0.3)
    st, _, _ = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/dyn/gone')
    check('dynamic route gone after disable', st == 404, 'status=%d' % st)
    smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, cookie=cookie)
    time.sleep(0.3)

    # ---- HostContext 聚合注入（槽位 7）----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/hostctx')
    r = json.loads(body)
    check('hostcontext injected (10/10 checks)', r.get('result') is True and r.get('score') == 10
          and r.get('pluginXid') == 'hello-sdk', body[:250])
    # settings 保存后上下文 option_table 刷新（不悬垂）
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/settings',
                                {'name': 'hello-sdk', 'config': {'welcomeMessage': 'ctx-refresh-test', 'showTime': True}}, cookie=cookie)
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/hostctx')
    r = json.loads(body)
    check('hostcontext alive after config swap', r.get('result') is True and r.get('score') == 10, body[:250])

    # ---- md4c（宿主新模块）：markdown 渲染一致性 ----
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/md-probe')
    r = json.loads(body)
    check('md4c renders markdown (h2/strong/li)', r.get('result') is True and '<h2>' in (r.get('html') or '')
          and '<strong>' in (r.get('html') or '') and '<li>' in (r.get('html') or ''), body[:250])

    # ---- 脚手架生成（auto_enable） ----
    st, _, body = smoke.request(PORT, 'POST', '/api/plugin/hello-sdk/generate')
    check('generate returns ok', json.loads(body).get('result') is True, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-gen/ping')
    check('generated plugin route alive (auto-enabled)', st == 200 and b'gen-ok' in body, 'status=%d body=%s' % (st, body[:80]))
    check('generated dir on disk', (target / 'plugin/hello-gen/main.c').exists() and (target / 'plugin/hello-gen/plugin.json').exists())
    gen_manifest = json.loads((target / 'plugin/hello-gen/plugin.json').read_text(encoding='utf-8'))
    check('generated manifest fields', gen_manifest.get('xid') == 'hello-gen' and gen_manifest.get('formatVersion') == 4
          and gen_manifest['build']['entry'] == 'main.c' and 'XADMIN_PLUGIN=1' in gen_manifest['build']['defines'])
    check('generated plugin in list', 'hello-gen' in plugin_names(cookie))

    # ---- 编译错误聚合落库 ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'badc'}, cookie=cookie)
    check('badc enable fails', json.loads(body).get('result') is False, body[:150])
    with sqlite3.connect(target / 'db/main.db') as db:
        row = db.execute("SELECT state, error_message FROM plugin_generation WHERE xid='badc' ORDER BY id DESC LIMIT 1").fetchone()
    check('failed generation row recorded', row and row[0] == 'failed', str(row))
    check('error message aggregated (multi-line diagnostics)', row and row[1] and ('error' in row[1].lower()) and len(row[1]) > 10,
          repr(row[1][:120]) if row else 'no row')

    # ---- settings：GET / schema 拒绝 / OnConfigChanged 回滚 ----
    st, _, body = smoke.request(PORT, 'GET', '/admin/plugin/settings?name=hello-sdk', cookie=cookie)
    r = json.loads(body)
    check('settings GET returns config', r.get('result') is True and 'welcomeMessage' in (r.get('config') or {}), body[:200])
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/settings',
                                {'name': 'hello-sdk', 'config': {'welcomeMessage': 12345, 'showTime': True}}, cookie=cookie)
    r = json.loads(body)
    check('schema rejects wrong type', r.get('result') is False and 'welcomeMessage' in r.get('message', ''), body[:200])
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/settings',
                                {'name': 'hello-sdk', 'config': {'welcomeMessage': 'reject-me', 'showTime': True}}, cookie=cookie)
    r = json.loads(body)
    check('OnConfigChanged reject triggers rollback', r.get('result') is False, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello-sdk/state')
    check('config restored after rollback', json.loads(body).get('message') != 'reject-me', body[:150])
    cfgfile = target / 'options/plugin/hello-sdk.json'
    if cfgfile.exists():
        saved = json.loads(cfgfile.read_text(encoding='utf-8'))
        check('config file restored after rollback', saved.get('welcomeMessage') != 'reject-me', str(saved))
    else:
        check('config file restored after rollback', True, '(no file — defaults)')
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/settings',
                                {'name': 'hello-sdk', 'config': {'welcomeMessage': 'saved-by-cap-e2e', 'showTime': True}}, cookie=cookie)
    check('valid config saves', json.loads(body).get('result') is True, body[:150])
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
warn = [l for l in log.splitlines() if 'warning' in l.lower() or ('error' in l.lower() and 'badc' not in l and '[tcc]' not in l)]
print('=' * 60)
print('unexpected warnings/errors:', warn if warn else 'NONE')
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
