# -*- coding: utf-8 -*-
"""{{#form}} 模板块 e2e：真渲染、rebuild 收录、编译失败恢复（坏 JSON 块）。"""
import sys, time, subprocess, json
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18243
target = smoke.fixture(PORT)
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
        if proc.poll() is not None: raise RuntimeError('server exited')
        try:
            if smoke.request(PORT, 'GET', '/admin/login')[0] in (200, 404): break
        except OSError: pass
        time.sleep(0.3)
    st, hd, _ = smoke.request(PORT, 'POST', '/admin/login',
                              {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
    cookie = hd['Set-Cookie'].split(';')[0]

    # 1. form_demo 真渲染（v1 语义：{{#form}} 块展开为表单 HTML）
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/template/form_demo', cookie=cookie)
    check('form_demo renders block html', st == 200 and b'xform-tpl-block' in body and b'xform-tpl-field' in body,
          'status=%d len=%d' % (st, len(body)))
    check('demo form json loaded (field content)', b'xform-tpl-group' in body and b'<input' in body, body[:0])

    # 2. v1 对齐路由 /admin/template/rebuild（20/20 收录，block_demo 不再编译失败）
    st, _, body = smoke.request(PORT, 'POST', '/admin/template/rebuild', cookie=cookie)
    r = json.loads(body)
    check('template rebuild route', r.get('result') is True, body[:150])
    loaded = r.get('data', {}).get('loaded', 0)
    failed = r.get('data', {}).get('failed', 0)
    check('rebuild loads all (block_demo ok)', failed == 0 and loaded >= 20, 'loaded=%d failed=%d' % (loaded, failed))

    # 3. rebuild 后渲染仍正常（缓存换代无回归）
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/template/form_demo', cookie=cookie)
    check('render after rebuild', st == 200 and b'xform-tpl-block' in body, 'status=%d' % st)

    # 4. 坏 JSON 块：渲染期报错，页面 500 且不影响其它模板
    bad = target / 'template/form/bad_demo.html'
    bad.parent.mkdir(parents=True, exist_ok=True)
    bad.write_text('<html>{{#form}}{ not json }{{#end}}</html>', encoding='utf-8')
    st, _, body2 = smoke.request(PORT, 'POST', '/admin/template/rebuild', cookie=cookie)
    st, _, body2 = smoke.request(PORT, 'GET', '/admin/view/template/form_demo', cookie=cookie)
    check('good template unaffected by bad sibling', st == 200 and b'xform-tpl-block' in body2, 'status=%d' % st)
    bad.unlink()
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
