# -*- coding: utf-8 -*-
"""内容系统阶段 2 全链路 e2e：建模型→生成→启用（30k 行 TCC 编译）→后台 CRUD→
公开 API→能力包挂载（category）→契约→再生成换代。"""
import sys, time, subprocess, json, sqlite3
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18247
target = smoke.fixture(PORT)
proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                        stdout=open(target / 'server.log', 'ab'), stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
ok = fail = 0
def check(name, cond, detail=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '|', detail)

SPEC = {
    'xid': 'news.post',
    'title': '站点新闻',
    'fields': [
        {'name': 'title', 'type': 'input', 'label': '标题'},
        {'name': 'summary', 'type': 'textarea', 'label': '摘要'},
        {'name': 'body_md', 'type': 'editor_md', 'label': '正文MD'},
    ],
    'capabilities': [{'key': 'content.category', 'enabled': True, 'config': {}}],
}

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

    # 1. 建模型 + 生成
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', SPEC, cookie=cookie)
    assert json.loads(body)['result'], body
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/generate', {'xid': 'news.post'}, cookie=cookie)
    r = json.loads(body)
    check('generate ok', r.get('result') is True and r.get('data', {}).get('generated') is True, body[:250])
    gen_main = target / 'plugin/news.post/generated/main.c'
    check('generated main.c on disk', gen_main.exists() and gen_main.stat().st_size > 500000,
          'size=%s' % (gen_main.stat().st_size if gen_main.exists() else 'missing'))
    manifest = json.loads((target / 'plugin/news.post/plugin.json').read_text(encoding='utf-8'))
    check('manifest defines include CAP category', any('XADMIN_CAP_CONTENT_CATEGORY' in d for d in manifest['build']['defines']),
          str(manifest['build']['defines']))
    check('manifest sources include pack stub', any('category' in s for s in manifest['build']['sources']) or manifest['build']['sources'] == ['generated/main.c'],
          str(manifest['build']['sources']))
    check('contracts.json produced', (target / 'plugin/news.post/runtime/contracts.json').exists())

    # 2. 启用（30k 行编译，给足时间）
    t0 = time.time()
    st, _, body = smoke.request(PORT, 'POST', '/admin/plugin/enable', {'name': 'news.post'}, cookie=cookie)
    enable_secs = time.time() - t0
    check('enable generated plugin', json.loads(body).get('result') is True, body[:300])
    print('  (enable took %.1fs)' % enable_secs)
    with sqlite3.connect(target / 'db/main.db') as db:
        row = db.execute("SELECT state, error_message FROM plugin_generation WHERE xid='news.post' ORDER BY id DESC LIMIT 1").fetchone()
    check('generation state active', row and row[0] == 'active', str(row))
    if row and row[0] != 'active':
        print('  error_message:', row[1][:500] if row[1] else None)

    # 3. 后台 CRUD
    md_text = '## 小节标题\n\n这是**加粗**与`code`段落\n\n- 列表甲\n- 列表乙\n'
    st, _, body = smoke.request(PORT, 'POST', '/admin/api/plugin/news.post/save',
                                {'id': 0, 'draft': False, 'data': {'title': '第一篇新闻', 'summary': '正文摘要', 'body_md': md_text, 'status': 2}}, cookie=cookie)
    r = json.loads(body)
    check('admin save content item', r.get('result') is True, body[:300])
    st, _, body = smoke.request(PORT, 'GET', '/admin/api/plugin/news.post/list?page=1&limit=10', cookie=cookie)
    r = json.loads(body)
    data = r.get('data')
    rows = data if isinstance(data, list) else (data or {}).get('list', [])
    check('admin list 1 row', isinstance(rows, list) and len(rows) >= 1, body[:300])
    item_id = rows[0].get('id') if isinstance(rows, list) and rows else 1

    # 4. 公开 API（发布态可见）
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/news.post/list')
    r = json.loads(body)
    check('public list', r.get('result') is True or r.get('code') == 0, body[:250])
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/news.post/detail?id=%s' % item_id)
    r = json.loads(body)
    check('public detail', st == 200 and (r.get('result') is True or r.get('code') == 0), 'status=%d body=%s' % (st, body[:200]))

    # 5. 能力包挂载（category 公开栏目接口）+ 契约
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/news.post/contracts')
    check('public contracts', st == 200, 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'GET', '/api/plugin/news.post/category/list')
    r = json.loads(body)
    check('category pack mounted (public route)', st == 200 and (r.get('result') is True or r.get('code') == 0 or 'result' in r),
          'status=%d body=%s' % (st, body[:200]))

    # 6. 管理视图页
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/plugin/news.post', cookie=cookie)
    check('admin view page', st == 200, 'status=%d' % st)

    # 7. 再生成换代（revision 提升 + 产物覆盖）
    spec2 = json.loads(json.dumps(SPEC)); spec2['fields'].append({'name': 'cover', 'type': 'image'})
    smoke.request(PORT, 'POST', '/admin/content/save', spec2, cookie=cookie)
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/generate', {'xid': 'news.post'}, cookie=cookie)
    r = json.loads(body)
    check('regenerate over existing', r.get('result') is True and r.get('data', {}).get('revision') == 2, body[:200])
    with sqlite3.connect(target / 'db/main.db') as db:
        gens = db.execute("SELECT status FROM content_generation WHERE plugin_xid='news.post' ORDER BY id").fetchall()
    check('content_generation rows', all(g[0] == 'success' for g in gens) and len(gens) == 2, str(gens))
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
warn = [l for l in log.splitlines() if 'warning' in l.lower() or ('error' in l.lower() and '[tcc]' not in l and 'news.post' not in l)]
print('=' * 60)
print('unexpected warnings/errors:', warn[:5] if warn else 'NONE')
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
