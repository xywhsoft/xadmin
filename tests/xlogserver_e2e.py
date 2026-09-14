# -*- coding: utf-8 -*-
"""xlogserver external API end-to-end: enable -> service -> task -> push -> verify."""
import sys, time, subprocess, json, sqlite3
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18235
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

    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'xlogserver'}, cookie=cookie)
    check('enable xlogserver', json.loads(body).get('result') is True, body[:200])

    # admin view page reachable (permission wiring ok)
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/plugin/xlogserver', cookie=cookie)
    check('admin view page 200', st == 200, 'status=%d' % st)

    # create service via admin API
    st, _, body = smoke.request(PORT, 'POST', '/admin/api/plugin/xlogserver/services',
                                {'name': '边缘采集', 'desc': 'e2e'}, cookie=cookie)
    r = json.loads(body)
    svc_id = r.get('data', {}).get('id')
    check('create service', r.get('result') is True and svc_id, body[:200])

    # ---- external APIs (no cookie) ----
    st, _, body = smoke.request(PORT, 'POST', '/api/v1/task/create',
                                {'service': svc_id, 'name': 'deploy-log', 'desc': 'from external api'})
    r = json.loads(body)
    task_id = r.get('data', {}).get('id')
    check('external task/create', r.get('code') == 0 and task_id, body[:200])

    st, _, body = smoke.request(PORT, 'POST', '/api/v1/log/push',
                                {'service': svc_id, 'task': task_id, 'class': 'info', 'text': 'hello from outside'})
    r = json.loads(body)
    log_id = r.get('data', {}).get('id')
    check('external log/push', r.get('code') == 0 and log_id, body[:200])

    # negatives
    st, _, body = smoke.request(PORT, 'POST', '/api/v1/log/push', {'service': 99999, 'task': 1})
    check('push unknown service rejected', json.loads(body).get('code') == 1, body[:200])
    st, _, body = smoke.request(PORT, 'POST', '/api/v1/task/create', {'service': svc_id})
    check('task/create without name rejected', json.loads(body).get('code') == 1, body[:200])


    # ---- 任务页渲染（宿主引擎 {{ }} 约定：变量代入 + JS 单花括号） ----
    st, _, body = smoke.request(PORT, 'GET',
                                '/admin/view/plugin/xlogserver/tasks?serviceId=%d' % svc_id, cookie=cookie)
    sub = ('var G_ServiceId = %d;' % svc_id).encode()
    check('tasks page rendered by host engine', st == 200 and sub in body and b'function(){' in body,
          'status=%d body=%s' % (st, body[:150]))
    check('no template syntax leak (no {{)', b'{{' not in body, body[:200])
    name_bytes = '边缘采集'.encode('utf-8')
    check('chinese serviceName rendered (no dangling 0xDD)', body.count(name_bytes) >= 2 and bytes([0xdd]) * 4 not in body,
          repr(body[body.find(b'margin: 0 12px;'):body.find(b'margin: 0 12px;') + 50]))
    st, _, body = smoke.request(PORT, 'GET',
                                '/admin/view/plugin/xlogserver/tasks/add?serviceId=%d' % svc_id, cookie=cookie)
    check('tasks add page rendered', st == 200 and b'html' in body.lower(), 'status=%d' % st)
    # services_edit / tasks_edit / tasks_logs（引擎渲染 + 无语法泄漏）
    st, _, body = smoke.request(PORT, 'GET',
                                '/admin/view/plugin/xlogserver/services/edit?id=%d' % svc_id, cookie=cookie)
    check('services edit page rendered', st == 200 and b'{{' not in body, 'status=%d body=%s' % (st, body[:120]))
    st, _, body = smoke.request(PORT, 'GET',
                                '/admin/view/plugin/xlogserver/tasks/edit?serviceId=%d&id=%d' % (svc_id, task_id), cookie=cookie)
    check('tasks edit page rendered', st == 200 and b'{{' not in body, 'status=%d body=%s' % (st, body[:120]))
    st, _, body = smoke.request(PORT, 'GET',
                                '/admin/view/plugin/xlogserver/tasks/logs?serviceId=%d&taskId=%d' % (svc_id, task_id), cookie=cookie)
    check('tasks logs page rendered', st == 200 and b'{{' not in body, 'status=%d body=%s' % (st, body[:120]))
    # admin task logs view shows the pushed log
    st, _, body = smoke.request(PORT, 'GET',
                                '/admin/api/plugin/xlogserver/task/logs?serviceId=%d&taskId=%d' % (svc_id, task_id),
                                cookie=cookie)
    r = json.loads(body)
    rows = r.get('data') or []
    check('pushed log visible via admin API',
          any(str(x.get('id')) == str(log_id) for x in rows), body[:300])

    # plugin private DB on disk
    check('plugin private db exists', (target / 'db/plugin/xlogserver/plugin.db').exists())
    logs_dir = target / 'plugin_data/xlogserver/logs'
    svc_dbs = list(logs_dir.glob('*.db')) if logs_dir.exists() else []
    check('service db created under plugin_data', len(svc_dbs) >= 1, str(logs_dir))
    if svc_dbs:
        with sqlite3.connect(svc_dbs[0]) as db:
            tasks = db.execute('SELECT COUNT(*) FROM tasks WHERE isDelete=0').fetchone()[0]
            logs_cnt = db.execute("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name LIKE 'log_%'").fetchone()[0]
        check('task row persisted', tasks == 1, 'tasks=%d' % tasks)
        check('log table created', logs_cnt >= 1, 'tables=%d' % logs_cnt)

    # auth ledger in main.db
    with sqlite3.connect(target / 'db/main.db') as db:
        ag = db.execute("SELECT COUNT(*) FROM authGroup WHERE plugin_xid='xlogserver' AND isDelete=0").fetchone()[0]
        au = db.execute("SELECT COUNT(*) FROM auth WHERE plugin_xid='xlogserver' AND isDelete=0").fetchone()[0]
        ur = db.execute("SELECT COUNT(*) FROM uris WHERE plugin_xid='xlogserver'").fetchone()[0]
    check('authGroup row', ag == 1, 'n=%d' % ag)
    check('auth row', au == 1, 'n=%d' % au)
    check('uris rows (10 admin; public routes unregistered per v1)', ur == 10, 'n=%d' % ur)
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
