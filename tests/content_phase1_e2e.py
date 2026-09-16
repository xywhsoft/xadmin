# -*- coding: utf-8 -*-
"""内容模型系统阶段 1 e2e：7 表 + 能力包装载 + 模型 CRUD/修订/体检 + 菜单 + 17 路由。"""
import sys, time, subprocess, json, sqlite3
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
PORT = 18246
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
    'xid': 'demo.article',
    'title': '演示文章模型',
    'fields': [
        {'name': 'title', 'type': 'input', 'label': '标题'},
        {'name': 'body', 'type': 'editor_html', 'label': '正文'},
    ],
    'capabilities': [
        {'key': 'content.category', 'enabled': True, 'config': {}},
    ],
}

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

    # ---- 启动装载：21 包入库 + 菜单 ----
    with sqlite3.connect(target / 'db/main.db') as db:
        q = lambda sql: db.execute(sql).fetchone()[0]
        packs = q("SELECT COUNT(*) FROM content_pack")
        tables = q("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name LIKE 'content_%'")
        menus = q("SELECT COUNT(*) FROM menu WHERE isDelete=0 AND href IN ('/admin/view/content','/admin/view/content/editor','/admin/view/content/packs','/admin/view/content/pack-store','/admin/view/content/page')")
    check('21 packs loaded into content_pack', packs == 21, 'packs=%d' % packs)
    check('7 content tables created', tables == 7, 'tables=%d' % tables)
    check('content menus ensured (4, v1 形态：editor 无菜单项)', menus == 4, 'menus=%d' % menus)

    # ---- 能力包 API ----
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/packs', cookie=cookie)
    r = json.loads(body)
    check('GET /admin/content/packs 21 items', r.get('result') is True and len(r.get('data', [])) == 21, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/pack?packId=content.comment', cookie=cookie)
    r = json.loads(body)
    detail = r.get('data') or {}
    check('pack detail with contracts json', r.get('result') is True and 'contractsJson' in detail and 'hooksJson' in detail, body[:150])
    # ---- 属性面板标准接口：服务端渲染 xform + 默认值 ----
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/pack/form?packId=content.comment', cookie=cookie)
    r = json.loads(body)
    fdata = r.get('data') or {}
    check('pack form renders xform html', st == 200 and r.get('result') is True
          and 'xform-tpl-block' in (fdata.get('html') or '')
          and 'name="moderation"' in (fdata.get('html') or ''), body[:150])
    check('pack form defaults extracted', len(fdata.get('defaults') or {}) >= 10
          and fdata['defaults'].get('moderation') == 'manual', str((fdata.get('defaults') or {}))[:120])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/pack/form?packId=nosuch.pack', cookie=cookie)
    check('pack form unknown pack rejected', json.loads(body).get('result') is False, body[:100])
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/pack/options',
                                {'packId': 'content.comment', 'options': {'moderation': 'pre'}}, cookie=cookie)
    check('pack options saved', json.loads(body).get('result') is True, body[:150])
    with sqlite3.connect(target / 'db/main.db') as db:
        opt = db.execute("SELECT options_json FROM content_pack_option WHERE pack_id='content.comment'").fetchone()
    check('pack options row', opt and 'moderation' in opt[0], str(opt))

    # ---- 模型 CRUD ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', SPEC, cookie=cookie)
    r = json.loads(body)
    check('save model revision 1', r.get('result') is True and r.get('data', {}).get('revision') == 1, body[:200])
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', SPEC, cookie=cookie)
    r = json.loads(body)
    check('identical save unchanged', r.get('data', {}).get('unchanged') is True, body[:200])
    spec2 = json.loads(json.dumps(SPEC)); spec2['fields'].append({'name': 'cover', 'type': 'image'})
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', spec2, cookie=cookie)
    r = json.loads(body)
    check('modified save revision 2', r.get('data', {}).get('revision') == 2, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/types', cookie=cookie)
    r = json.loads(body)
    items = r.get('data', {}).get('items', [])
    check('list models 1 item / fieldCount 3', r.get('result') and len(items) == 1 and items[0]['fieldCount'] == 3, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/type?xid=demo.article', cookie=cookie)
    r = json.loads(body)
    check('get model by xid', r.get('result') is True and r.get('data', {}).get('xid') == 'demo.article', body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/revisions?xid=demo.article', cookie=cookie)
    r = json.loads(body)
    check('revisions listed (2)', len(r.get('data', [])) == 2, body[:200])
    with sqlite3.connect(target / 'db/main.db') as db:
        mp = db.execute("SELECT pack_id, enabled FROM content_model_pack").fetchall()
    check('model-pack row synced', mp == [('content.category', 1)], str(mp))

    # ---- 属性面板值往返：能力 config 随修订落盘（xform 收集形态） ----
    spec3 = json.loads(json.dumps(SPEC))
    spec3['capabilities'] = [{'key': 'content.comment', 'enabled': True,
                              'config': {'moderation': 'manual', 'allowPublicPost': True, 'minBodyLength': '5'}}]
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', spec3, cookie=cookie)
    r = json.loads(body)
    check('capability config save', r.get('result') is True, body[:200])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/type?xid=demo.article', cookie=cookie)
    saved = json.loads(json.loads(body)['data']['specJson']) if isinstance(json.loads(body).get('data', {}).get('specJson'), str) else json.loads(body).get('data', {}).get('specJson', {})
    saved_cfg = ((saved.get('capabilities') or [{}])[0]).get('config', {})
    check('capability config roundtrip', saved_cfg.get('minBodyLength') == '5'
          and saved_cfg.get('moderation') == 'manual' and saved_cfg.get('allowPublicPost') is True, str(saved_cfg)[:150])

    # ---- 校验负例 ----
    bad = json.loads(json.dumps(SPEC)); bad['fields'][1]['name'] = 'title'
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', bad, cookie=cookie)
    check('duplicate field rejected', json.loads(body).get('result') is False and 'duplicate' in json.loads(body).get('message', ''), body[:200])
    bad2 = json.loads(json.dumps(SPEC)); bad2['capabilities'] = [{'key': 'content.nosuch'}]
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/save', bad2, cookie=cookie)
    check('unknown capability rejected', json.loads(body).get('result') is False and 'unknown' in json.loads(body).get('message', ''), body[:200])

    # ---- advisor ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/advisor', SPEC, cookie=cookie)
    r = json.loads(body)
    advisor = r.get('data') or {}
    check('advisor ok status', advisor.get('status') == 'ok' and advisor.get('errorCount') == 0, body[:200])
    check('advisor has pack warning + acceptance', any(i.get('level') == 'warning' for i in advisor.get('items', []))
          and any(i.get('kind') == 'acceptance' for i in advisor.get('items', [])), body[:300])
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/advisor', {'xid': 'x!', 'fields': []}, cookie=cookie)
    advisor = json.loads(body).get('data') or {}
    check('advisor errors on bad spec', advisor.get('status') == 'error' and advisor.get('errorCount', 0) >= 2, body[:200])

    # ---- 视图页 + generate 占位 ----
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/content', cookie=cookie)
    check('content index page 200', st == 200 and b'html' in body.lower(), 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/content/editor', cookie=cookie)
    check('editor page 200', st == 200, 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'GET', '/admin/view/content/packs', cookie=cookie)
    check('packs page 200', st == 200, 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/generate', {'xid': 'demo.article'}, cookie=cookie)
    r = json.loads(body)
    check('generate succeeds (phase 2 wired)', r.get('result') is True and r.get('data', {}).get('generated') is True, body[:200])

    # ---- R3/R4 静态烘焙：生成物零运行时配置读取 ----
    gen_main = (target / 'plugin/demo.article/generated/main.c').read_text(encoding='utf-8', errors='replace')
    runtime_reads = ['Managed_AbilityPackConfigInt("', 'Managed_AbilityPackConfigBool("',
                     'Managed_AbilityPackConfigTextDup("', 'Managed_SearchWeight("',
                     'Managed_AbilityRoutePrefixDup("', 'Managed_SeoConfigText(tblConfig',
                     'Managed_AbilityPackConfigArrayDup("']
    leftover = [pat for pat in runtime_reads if pat in gen_main]
    check('R3 baked: zero runtime config reads', not leftover, 'leftover=%s' % leftover)
    check('R3 baked: constants present', '((int)' in gen_main and 'xrtStrDup("' in gen_main,
          'baked const markers missing')
    check('R4 baked: ui helpers static', '(void)tblSpec;' in gen_main.split('Managed_GetUiListPageSize')[1][:200],
          'pageSize body not baked')
    check('R3 helpers emitted', 'Managed_BakedSeoTemplateText(const char* sKey)' in gen_main, 'seo value table missing')

    # ---- 删除 ----
    st, _, body = smoke.request(PORT, 'POST', '/admin/content/delete', {'xid': 'demo.article'}, cookie=cookie)
    check('model deleted', json.loads(body).get('result') is True, body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/admin/content/type?xid=demo.article', cookie=cookie)
    check('model gone after delete', json.loads(body).get('result') is False, body[:150])
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
warn = [l for l in log.splitlines() if 'warning' in l.lower() or 'error' in l.lower()]
print('=' * 60)
print('warnings/errors:', warn if warn else 'NONE')
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
