# -*- coding: utf-8 -*-
"""阶段 3 插件复活 e2e：hello（资源体系）+ perfmon（Windows 系统指标，iphlpapi 实链）。"""
import sys, time, subprocess, json
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18261
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
        if proc.poll() is not None: raise RuntimeError((target / 'server.log').read_text(encoding='utf-8', errors='replace')[-1500:])
        try:
            if smoke.request(PORT, 'GET', '/admin/login')[0] in (200, 404): break
        except OSError: pass
        time.sleep(0.3)
    st, hd, _ = smoke.request(PORT, 'POST', '/admin/login',
                              {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
    cookie = hd['Set-Cookie'].split(';')[0]

    # ---- hello ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'hello'}, cookie=cookie)
    check('enable hello', json.loads(body).get('result') is True, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello/greeting')
    check('hello greeting (hook+service)', st == 200 and json.loads(body).get('result') is True, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/plugin/hello', cookie=cookie)
    check('hello admin page (LoadPluginPage)', st == 200 and b'html' in body.lower(), 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/hello/info')
    r = json.loads(body)
    check('hello info (db/options injected)', r.get('result') is True and r.get('dbInjected') and r.get('optionsInjected'), body[:200])

    # ---- perfmon ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'perfmon'}, cookie=cookie)
    check('enable perfmon', json.loads(body).get('result') is True, body[:200])
    time.sleep(1.0)
    st, _, body = smoke.request(PORT, 'GET', '/admin/api/plugin/perfmon/current', cookie=cookie)
    r = json.loads(body)
    d = r.get('data') or {}
    check('perfmon current (real metrics)', r.get('result') is True and d.get('memory_total', 0) > 0, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/admin/api/plugin/perfmon/serverinfo', cookie=cookie)
    r = json.loads(body)
    d = r.get('data') or {}
    check('perfmon serverinfo (GetAdaptersInfo+sysinfo)', r.get('result') is True and d.get('cpu_cores', 0) > 0, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/admin/api/plugin/perfmon/disk', cookie=cookie)
    r = json.loads(body)
    check('perfmon disk', r.get('result') is True and len(r.get('data') or []) >= 1, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/admin/api/plugin/perfmon/history', cookie=cookie)
    check('perfmon history (采集入库)', json.loads(body).get('result') is True, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/plugin/perfmon', cookie=cookie)
    check('perfmon view', st == 200, 'status=%d' % st)
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
warn = [l for l in log.splitlines() if 'warning' in l.lower() or 'error' in l.lower()]
print('=' * 60)
print('warnings/errors:', warn[:5] if warn else 'NONE')
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
