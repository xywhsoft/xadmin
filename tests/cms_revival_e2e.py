# -*- coding: utf-8 -*-
"""阶段 3：存量生成插件 cms.article 复活验证（slug/like/category/comment 等 22 包全挂载）。"""
import sys, time, subprocess, json, sqlite3
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18250
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
            raise RuntimeError((target / 'server.log').read_text(encoding='utf-8', errors='replace')[-2000:])
        try:
            if smoke.request(PORT, 'GET', '/admin/login')[0] in (200, 404): break
        except OSError: pass
        time.sleep(0.3)
    st, hd, _ = smoke.request(PORT, 'POST', '/admin/login',
                              {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
    cookie = hd['Set-Cookie'].split(';')[0]

    t0 = time.time()
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'cms.article'}, cookie=cookie)
    took = time.time() - t0
    check('enable cms.article (22 packs)', json.loads(body).get('result') is True, body[:200])
    print('  (enable took %.1fs)' % took)
    if 'result\\":true' not in body.decode('utf-8', 'ignore') and b'true' not in body:
        with sqlite3.connect(target / 'db/main.db') as db:
            row = db.execute("SELECT error_message FROM plugin_generation WHERE xid='cms.article' ORDER BY id DESC LIMIT 1").fetchone()
        if row and row[0]:
            print('  ERR:', row[0][:600])

    # 核心后台 CRUD
    st, _, body = smoke.request(PORT, 'POST', '/admin/api/plugin/cms.article/save',
                                {'id': 0, 'draft': False, 'data': {'title': '复兴首篇', 'summary': 'cms.article 复活', 'status': 2}}, cookie=cookie)
    check('admin save', json.loads(body).get('result') is True, body[:250])
    st, _, body = smoke.request(PORT, 'GET', '/admin/api/plugin/cms.article/list?page=1&limit=5', cookie=cookie)
    r = json.loads(body)
    data = r.get('data')
    rows = data if isinstance(data, list) else (data or {}).get('list', [])
    check('admin list', isinstance(rows, list) and len(rows) >= 1, body[:250])
    item_id = rows[0].get('id') if rows else 1

    # 公开 API
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/list')
    check('public list', st == 200 and json.loads(body).get('result') is not False, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/detail?id=%s' % item_id)
    check('public detail', st == 200 and json.loads(body).get('result') is not False, body[:200])

    # slug 包：伪静态解析路由（动态路由）
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/slug/resolve?slug=first-post')
    check('slug pack route', st in (200, 404), 'status=%d' % st)
    # like 包
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/like/status?id=%s' % item_id)
    check('like pack route', st == 200, 'status=%d body=%s' % (st, body[:120]))
    # comment 包
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/comment/list?id=%s' % item_id)
    check('comment pack route', st == 200, 'status=%d' % st)
    # category 包
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/category/list')
    check('category pack route', st == 200, 'status=%d' % st)
    # 契约
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/cms.article/contracts')
    check('contracts', st == 200 and b'abilityPacks' in body, 'status=%d' % st)
    # 管理视图
    st, _, _ = smoke.request(PORT, 'GET', '/admin/view/plugin/cms.article', cookie=cookie)
    check('admin view', st == 200, 'status=%d' % st)
    # 注册的路由规模（台账）
    with sqlite3.connect(target / 'db/main.db') as db:
        n = db.execute("SELECT COUNT(*) FROM plugin_resource WHERE xid='cms.article' AND resource_type='route' AND status='active'").fetchone()[0]
        a = db.execute("SELECT COUNT(*) FROM auth WHERE plugin_xid='cms.article' AND isDelete=0").fetchone()[0]
    check('routes registered (100+)', n >= 100, 'routes=%d' % n)
    check('auths registered (30+)', a >= 30, 'auths=%d' % a)
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
warn = [l for l in log.splitlines() if 'warning' in l.lower() or ('error' in l.lower() and 'cms.article' not in l)]
print('=' * 60)
print('warnings/unexpected:', warn[:5] if warn else 'NONE')
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
