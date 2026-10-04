"""Run xs against an isolated SQLite backup; never modify migrated user data.

The fixture contains disposable credentials only and listens on loopback.
--keep-running retains a preview server; its PID and directory are printed.
"""
from pathlib import Path
import argparse
import hashlib
import http.client
import json
import os
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from write_regression import checks as write_checks

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXECUTABLE = ROOT / ('xs.exe' if os.name == 'nt' else 'xs')
USER = 'migration_smoke'
PASSWORD = 'Temporary-test-only-9081'


def client_hash(user, password):
    return hashlib.sha256((user + '_xywhsoft_' + password).encode()).hexdigest()


def legacy_now():
    return int(time.time()) + 62167219200 + int(datetime.now().astimezone().utcoffset().total_seconds())


def verify_assets():
    entries = json.loads((ROOT / 'docs/v1-assets.json').read_text())
    for entry in entries:
        for key in ('source', 'target'):
            expect = entry['sha256']
            # 有意偏差（如 R1 登出改 POST）以 target_sha256 显式记录目标哈希；源基线不变。
            if key == 'target' and 'target_sha256' in entry:
                expect = entry['target_sha256']
            assert hashlib.sha256((ROOT / entry[key]).read_bytes()).hexdigest() == expect, entry[key]
    print('PASS', len(entries), 'unchanged v1 UI/plugin assets')


def fixture(port, protected=False, register_interval=0):
    base = ROOT / 'tests/.runtime'
    base.mkdir(parents=True, exist_ok=True)
    target = Path(tempfile.mkdtemp(prefix='smoke-', dir=base))
    for name in ('wwwroot', 'page', 'template', 'options', 'forms', 'plugin_sdk', 'install', 'capability-pack', 'content', 'site'):
        shutil.copytree(ROOT / name, target / name)
    shutil.copytree(ROOT / 'plugin', target / 'plugin')
    shutil.copytree(ROOT / 'tests/plugins/hello-sdk', target / 'plugin/hello-sdk')
    (target / 'plugin_data').mkdir(exist_ok=True)
    for name in ():
        shutil.copytree(ROOT / name, target / name)
    # The source may have a concealed admin entry. Only the disposable fixture
    # disables it; dedicated tests cover the protected-entry setting separately.
    global_path = target / 'options/global.json'
    options = json.loads(global_path.read_text(encoding='utf-8-sig'))
    for group in options.get('classList', []):
        for option in group.get('options', []):
            if option.get('name') == 'cp_url':
                option['value'] = '/smoke-private-entry' if protected else ''
            if option.get('name') == 'mail_enabled':
                # A backup may contain real queued messages. Fixtures never
                # inherit permission to deliver those messages externally.
                option['value'] = False
    if register_interval is not None:
        options.setdefault('classList', []).append(
            {'title': 'soak', 'options': [
                {'name': 'registerIntervalSecond', 'value': str(register_interval),
                 'title': 'register throttle', 'type': 'text'}]})
    global_path.write_text(json.dumps(options), encoding='utf-8')
    (target / 'db').mkdir()
    (target / 'temp').mkdir()
    (target / 'logs').mkdir()
    with sqlite3.connect((ROOT / 'db/main.db').as_uri() + '?mode=ro', uri=True) as src:
        with sqlite3.connect(target / 'db/main.db') as dst:
            src.backup(dst)
            salt = 'migration-test-salt'
            pwd = hashlib.sha256((USER + salt + client_hash(USER, PASSWORD)).encode()).hexdigest().upper()
            now = legacy_now()
            dst.execute('INSERT INTO user(user,salt,pwd,role,authLevel,createTime,updateTime,isDelete) '
                        'VALUES(?,?,?,1,999,?,?,0)', (USER, salt, pwd, now, now))
            role = dst.execute('INSERT INTO role(name,desc,authList,authLevel,createTime,updateTime,isDelete) '
                               'VALUES(?,?,?,0,?,?,0)', ('smoke-denied', '', '[]', now, now)).lastrowid
            user = USER + '_denied'
            pwd = hashlib.sha256((user + salt + client_hash(user, PASSWORD)).encode()).hexdigest().upper()
            dst.execute('INSERT INTO user(user,salt,pwd,role,authLevel,createTime,updateTime,isDelete) '
                        'VALUES(?,?,?,?,0,?,?,0)', (user, salt, pwd, role, now, now))
            dst.commit()
    # 夹具归一化：插件启用状态不继承根库（用户态）——测试自管（hello-sdk 由
    # 用例经 API 启用）。否则 xlogserver 等自启插件会把多代重载时序竞态带入
    # 全部 smoke 轮次（间歇挂死，storm 门禁单测无法覆盖完整序列）。
    with sqlite3.connect(target / 'db/main.db') as db:
        db.execute('UPDATE plugin_runtime SET enabled=0')
        db.execute('UPDATE sched_task SET enabled=0')
        db.commit()

    config = json.loads((ROOT / 'xs.json').read_text(encoding='utf-8'))
    service = config['services'][0]
    service['port'] = port
    host = service['host_default']
    host['devfile'] = str(ROOT / 'tests/host.c')
    host['dev_inc'] = str(ROOT / 'includes')
    host['dev_lib'] = str(ROOT / 'librarys')
    (target / 'xs.json').write_text(json.dumps(config), encoding='utf-8')
    return target


CSRF = {}

def request(port, method, path, data=None, cookie=None, extra_headers=None):
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    headers = {}
    if data is not None:
        data = json.dumps(data).encode() if not isinstance(data, bytes) else data
        headers['Content-Type'] = 'application/json'
    if cookie:
        headers['Cookie'] = cookie
        if cookie in CSRF and method not in ('GET','HEAD'):
            headers['X-CSRF-Token'] = CSRF[cookie]
    if extra_headers:
        headers.update(extra_headers)
    try:
        conn.request(method, path, data, headers)
        resp = conn.getresponse()
        body = resp.read()
        response_headers = dict(resp.getheaders())
        for key, value in resp.getheaders():
            if key.lower() == 'set-cookie' and value.startswith('MSID='):
                response_headers['Set-Cookie'] = value
        if path == '/api/v1/login' and resp.status == 200:
            result = json.loads(body)
            if result.get('code') == 0 and 'Set-Cookie' in response_headers:
                CSRF[response_headers['Set-Cookie'].split(';')[0]] = result['data']['csrf_token']
        return resp.status, response_headers, body
    finally:
        conn.close()


def checks(port, target, protected=False, functional_only=False):
    login_path = '/smoke-private-entry' if protected else '/admin/login'
    status, headers, body = request(port, 'GET', login_path)
    assert status == 200 and b'<html' in body.lower(), (status, body[:200])
    print('PASS login page')
    status, headers, body = request(port, 'GET', '/admin')
    assert (status == 404 if protected else status == 302 and headers.get('Location') == login_path), (status, headers)
    if protected:
        assert request(port, 'GET', '/admin/login')[0] == 404
    print('PASS unauthenticated entry policy')
    status, headers, body = request(port, 'POST', login_path, b'{invalid')
    assert status == 200 and json.loads(body)['result'] is False
    status, headers, body = request(port, 'POST', login_path, {
        'username': USER, 'password': client_hash(USER, PASSWORD)})
    assert status == 200 and json.loads(body)['result'], (status, body[:300])
    cookie = headers['Set-Cookie'].split(';')[0]
    assert len(cookie.split('=', 1)[1]) == 32 and 'HttpOnly' in headers['Set-Cookie'] and 'SameSite=Lax' in headers['Set-Cookie']
    print('PASS login with v1 password format')
    for path in ('/admin', '/admin/menu', '/admin/view/home', '/admin/view/auth/user',
                 '/admin/view/auth/user/add',
                 '/admin/auth/user?page=1&limit=10'):
        status, headers, body = request(port, 'GET', path, cookie=cookie)
        assert status == 200 and body, (path, status, body[:200])
        # Static list pages deliberately contain Layui client-side templates.
        assert b'{{#foreach' not in body, (path, 'unrendered server template')
        print('PASS', path)
    # Existing HTML resources must be returned byte-for-byte, not rewritten.
    for url, file in (('/admin/view/auth/user', 'page/auth/user.html'), ('/layui/layui.js', 'wwwroot/layui/layui.js')):
        status, _, body = request(port, 'GET', url, cookie=cookie)
        assert status == 200 and body == (ROOT / file).read_bytes(), url
    print('PASS original page/static bytes')
    for path in ('/main.c', '/route.h', '/db/main.db', '/options/global.json',
                 '/page/admin/login.html', '/template/auth/user_add.html',
                 '/../db/main.db', '/%2e%2e/db/main.db', '/dev/v1/hosts/xadmin/data/db/main.db'):
        status, _, body = request(port, 'GET', path, cookie=cookie)
        assert status in (400, 403, 404) and b'SQLite format' not in body, (path, status)
    print('PASS private files outside static root')
    # Framework method slots and patterns: probes are absent from production.
    for method, path, expected in (
        ('GET', '/__test/method', b'replacement'), ('POST', '/__test/method', b'post'),
        ('GET', '/__test/item/new', b'get:static'), ('GET', '/__test/item/42?ignored=yes', b'get:42'),
        ('POST', '/__test/item/42', b'replacement')):
        status, _, body = request(port, method, path)
        assert status == 200 and body == expected, (method, path, status, body)
    status, headers, _ = request(port, 'DELETE', '/__test/item/42')
    assert status == 405 and headers.get('Allow') == 'GET, POST', (status, headers)
    assert request(port, 'HEAD', '/__test/item/42')[0] == 405
    for method in ('GET', 'POST', 'PUT', 'DELETE', 'PATCH'):
        assert request(port, method, '/__test/crud')[0] == 200
    assert request(port, 'OPTIONS', '/__test/crud')[0] == 405
    print('PASS dynamic/static routing, repeated slots, CRUD and 405/Allow')
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        chunks = [b'hello ', b'chunked ', b'body']
        conn.request('POST', '/__test/echo', body=iter(chunks), encode_chunked=True)
        resp = conn.getresponse()
        assert resp.status == 200 and resp.read() == b''.join(chunks)
        conn.request('GET', '/__test/method')
        resp = conn.getresponse()
        assert resp.status == 200 and resp.read() == b'replacement'
    finally:
        conn.close()
    print('PASS chunked body and next keep-alive request')
    # JSON CRUD writes only the disposable database; inspect the actual row too.
    status, _, body = request(port, 'POST', '/admin/auth/user', {
        'username': 'smoke_added', 'password': client_hash('smoke_added', PASSWORD), 'role': 1}, cookie)
    created = json.loads(body)
    assert status == 200 and created['result'], (status, body)
    uid = created['data']['id']
    status, _, body = request(port, 'PUT', '/admin/auth/user', {'id': uid, 'role': 1, 'authLevel': 12}, cookie)
    assert status == 200 and json.loads(body)['result']
    status, _, body = request(port, 'GET', '/admin/auth/user?search=smoke_added', cookie=cookie)
    rows = json.loads(body)['data']
    assert len(rows) == 1 and rows[0]['authLevel'] == 12 and 'pwd' not in rows[0] and 'salt' not in rows[0], rows
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute('select authLevel,isDelete from user where id=?', (uid,)).fetchone() == (12, 0)
        now = db.execute('select createTime from user where id=?', (uid,)).fetchone()[0]
        assert abs(now - time.time() * 1000000) < 10000000, now
    status, _, body = request(port, 'GET', '/admin/view/auth/user/edit?id=' + str(uid), cookie=cookie)
    assert status == 200 and b'smoke_added' in body
    status, _, body = request(port, 'DELETE', '/admin/auth/user?id=' + str(uid), cookie=cookie)
    assert status == 200 and json.loads(body)['result']
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute('select isDelete from user where id=?', (uid,)).fetchone()[0] == 1
    print('PASS administrator CRUD, template edit, old time format, soft deletion')
    # Read regression for all connected management families, including nested
    # authorization templates. Write parity is tracked separately in the docs.
    for path in ('/admin/auth/role', '/admin/auth/group', '/admin/auth/auth', '/admin/auth/uris',
                 '/admin/option/menu', '/admin/member/user', '/admin/member/group',
                 '/admin/member/auth', '/admin/member/authgroup', '/admin/logs'):
        status, _, body = request(port, 'GET', path + '?page=1&limit=10', cookie=cookie)
        assert status == 200 and isinstance(json.loads(body), dict), (path, status, body[:150])
    for path in ('/admin/view/auth/role/add', '/admin/view/auth/role/edit?id=1',
                 '/admin/view/auth/group/add', '/admin/view/auth/auth/add',
                 '/admin/view/auth/uris/edit?id=1', '/admin/view/member/user/add',
                 '/admin/view/member/group/add', '/admin/view/member/auth/add',
                 '/admin/view/option/menu/add', '/admin/view/logs'):
        status, _, body = request(port, 'GET', path, cookie=cookie)
        assert status == 200 and len(body) > 100 and b'{{#foreach' not in body, (path, status, body[:150])
    print('PASS connected management reads and nested templates')
    # v1 trace/debug family: cache overview, session dump, option dump, auth
    # caches with type filter, and the route table incl. dynamic patterns.
    assert request(port, 'POST', '/admin/trace')[0] == (404 if protected else 302)
    status, _, body = request(port, 'GET', '/admin/trace', cookie=cookie)
    result = json.loads(body)
    assert status == 200 and result['result'] and result['data']['installed'] is True, result
    assert result['data']['database']['connected'] is True
    assert result['data']['option']['exists'] and result['data']['option']['count'] >= 1
    assert result['data']['auth']['auth_count'] >= 1 and result['data']['auth']['role_count'] >= 1
    assert result['data']['route']['count'] >= 66, result['data']['route']
    assert result['data']['adminSession']['count'] >= 1, result['data']['adminSession']
    status, _, body = request(port, 'POST', '/admin/trace', cookie=cookie)
    assert status == 404
    status, _, body = request(port, 'GET', '/admin/trace/session', cookie=cookie)
    result = json.loads(body)
    assert result['result'] and len(result['data']['admin']) >= 1, result
    assert any(v.get('user') == USER for v in result['data']['admin'].values())
    status, _, body = request(port, 'GET', '/admin/trace/option', cookie=cookie)
    result = json.loads(body)
    assert result['result'] and 'adminTitle' in result['data']['global'], result
    for query, keys in (('?type=all', {'roleAuth', 'auth', 'group', 'role'}), ('?type=role', {'role'})):
        status, _, body = request(port, 'GET', '/admin/trace/auth' + query, cookie=cookie)
        result = json.loads(body)
        assert status == 200 and result['result'] and set(result['data']) == keys, (query, result)
    status, _, body = request(port, 'GET', '/admin/trace/route', cookie=cookie)
    result = json.loads(body)
    assert result['result'], result
    routes = {row['uri']: row for row in result['data']}
    assert routes['/admin/trace']['authId'] == 20 and routes['/admin/trace']['auth'] is True
    assert routes['/admin/tool/reload/host']['authId'] == 1
    assert '/__test/item/{id}' in routes
    print('PASS trace overview/session/option/auth/route dumps')
    # option tool: v1 admin-entry candidate generator (GET-only, no state change).
    assert request(port, 'POST', '/admin/option/tool/admin-entry')[0] == (404 if protected else 302)
    status, _, body = request(port, 'POST', '/admin/option/tool/admin-entry', cookie=cookie)
    assert status == 404
    candidates = []
    for _ in range(2):
        status, _, body = request(port, 'GET', '/admin/option/tool/admin-entry', cookie=cookie)
        result = json.loads(body)
        assert status == 200 and result['result'] is True and result['message'] == 'ok', result
        candidate = result['data']['path']
        assert candidate.startswith('/') and len(candidate) == 33 and candidate[1:].isalnum(), candidate
        candidates.append(candidate)
    assert candidates[0] != candidates[1]
    status, _, body = request(port, 'GET', '/admin/trace/route', cookie=cookie)
    assert candidates[0] not in {row['uri'] for row in json.loads(body)['data']}
    print('PASS option tool admin-entry generation')
    # form engine + dynamic-form option editing (v1 /admin/form + /admin/option).
    assert request(port, 'POST', '/admin/form')[0] == (404 if protected else 302)
    status, _, body = request(port, 'GET', '/admin/view/form', cookie=cookie)
    assert status == 200 and body == (ROOT / 'page/form.html').read_bytes()
    status, _, body = request(port, 'GET', '/admin/form?file=..%2Fglobal.json', cookie=cookie)
    assert status == 200 and json.loads(body)['result'] is False
    status, _, body = request(port, 'GET', '/admin/form', cookie=cookie)
    result = json.loads(body)
    assert result['result'] and result['data']['file'] == 'demo_form.json' and result['data']['source'] == 'form', result
    schema = result['data']['schema']
    fields = [f for g in schema['groups'] for f in g['fields']]
    required = [f['name'] for f in fields if f.get('required')]
    assert required and result['data']['fieldTypes']['types']
    demo_values = dict(result['data']['values'])
    assert all(k in demo_values for k in required)
    status, _, body = request(port, 'POST', '/admin/form', {
        'file': 'demo_form.json', 'source': 'form', 'data': {}}, cookie=cookie)
    result = json.loads(body)
    assert status == 200 and result['result'] is False and '为必填项' in result['message'], result
    demo_values[required[0]] = 'smoke-demo-value'
    status, _, body = request(port, 'POST', '/admin/form', {
        'file': 'demo_form.json', 'source': 'form', 'data': demo_values}, cookie=cookie)
    assert json.loads(body)['result'], body
    status, _, body = request(port, 'GET', '/admin/form', cookie=cookie)
    assert json.loads(body)['data']['values'][required[0]] == 'smoke-demo-value'
    status, headers, _ = request(port, 'GET', '/admin/view/option?file=global.json', cookie=cookie)
    assert status == 302 and headers.get('Location') == '/admin/view/form?source=option&file=global.json'
    status, _, body = request(port, 'GET', '/admin/option?file=global.json', cookie=cookie)
    result = json.loads(body)
    assert result['result'] and result['data']['source'] == 'option', result
    names = [f['name'] for g in result['data']['schema']['groups'] for f in g['fields']]
    assert 'cp_url' in names and 'adminTitle' in names, names
    saved = dict(result['data']['values'])
    saved['adminTitle'] = 'smoke-option-title'
    status, _, body = request(port, 'POST', '/admin/option', {
        'file': 'global.json', 'source': 'option', 'data': saved}, cookie=cookie)
    assert json.loads(body)['result'], body
    status, _, body = request(port, 'GET', '/brand/admin')
    assert json.loads(body)['data']['adminTitle'] == 'smoke-option-title'
    with open(target / 'options/global.json', encoding='utf-8-sig') as f:
        on_disk = json.load(f)
    disk_title = [o['value'] for c in on_disk['classList'] for o in c['options'] if o['name'] == 'adminTitle']
    assert disk_title == ['smoke-option-title'], disk_title
    rejected = dict(saved)
    rejected['cp_url'] = '/admin'
    status, _, body = request(port, 'POST', '/admin/option', {
        'file': 'global.json', 'source': 'option', 'data': rejected}, cookie=cookie)
    assert json.loads(body)['result'] is False, body
    # /admin/form with source=option must save through the option path (v1 contract).
    status, _, body = request(port, 'GET', '/admin/option?file=example.json', cookie=cookie)
    example = json.loads(body)
    assert example['result'], example
    example_values = dict(example['data']['values'])
    example_values['input_text'] = 'smoke-form-option'
    status, _, body = request(port, 'POST', '/admin/form', {
        'file': 'example.json', 'source': 'option', 'data': example_values}, cookie=cookie)
    assert json.loads(body)['result'], body
    on_disk = json.loads((target / 'options/example.json').read_text(encoding='utf-8-sig'))
    disk_val = [o['value'] for c in on_disk['classList'] for o in c['options'] if o['name'] == 'input_text']
    assert disk_val == ['smoke-form-option'], disk_val
    print('PASS form schema/demo save and dynamic option editing round-trip')
    # option file management: list, definition CRUD, locked guard, menu linkage.
    assert request(port, 'GET', '/admin/option/files')[0] == (404 if protected else 302)
    status, _, body = request(port, 'GET', '/admin/view/option/files', cookie=cookie)
    assert status == 200 and body == (ROOT / 'page/option/files.html').read_bytes()
    status, _, body = request(port, 'GET', '/admin/view/option/file', cookie=cookie)
    assert status == 200 and body == (ROOT / 'page/option/file_edit.html').read_bytes()
    status, _, body = request(port, 'POST', '/admin/option/files', cookie=cookie)
    assert status == 404
    status, _, body = request(port, 'GET', '/admin/option/files', cookie=cookie)
    result = json.loads(body)
    assert result['result'] and isinstance(result['data'], list), result
    files = {row['file']: row for row in result['data']}
    assert set(files) == {'global.json', 'example.json', 'attachment.json'}, files
    assert files['global.json']['locked'] is True and files['global.json']['canDelete'] is False
    assert files['global.json']['inMenu'] is True and files['global.json']['menuTitle']
    assert files['example.json']['inMenu'] is False
    status, _, body = request(port, 'GET', '/admin/option/file?file=global.json', cookie=cookie)
    result = json.loads(body)
    assert result['result'] and result['data']['namespace'] == 'global' and result['data']['classList']
    status, _, body = request(port, 'GET', '/admin/option/file?file=..%2F..%2Fdb.json', cookie=cookie)
    assert json.loads(body)['result'] is False
    definition = {
        'namespace': 'smoke_opt', 'title': 'Smoke Opt', 'desc': 'disposable',
        'classList': [{'title': 'G1', 'options': [
            {'name': 'demo_flag', 'type': 'switch', 'title': 'Demo Flag', 'value': False},
            {'name': 'demo_text', 'type': 'text', 'title': 'Demo Text', 'value': 'a'}]}]}
    status, _, body = request(port, 'POST', '/admin/option/file',
                              {'file': 'smoke_opt.json', 'data': definition}, cookie=cookie)
    assert json.loads(body)['result'] is True, body
    assert (target / 'options/smoke_opt.json').exists()
    status, _, body = request(port, 'POST', '/admin/option/file',
                              {'file': 'smoke_opt.json', 'data': definition}, cookie=cookie)
    result = json.loads(body)
    assert result['result'] is False and '已存在' in result['message'], result
    status, _, body = request(port, 'POST', '/admin/option/file',
                              {'file': 'smoke_opt2.json', 'data': definition}, cookie=cookie)
    result = json.loads(body)
    assert result['result'] is False and 'namespace' in result['message'], result
    edited = json.loads(json.dumps(definition))
    edited['classList'][0]['options'].append({'name': 'demo_int', 'type': 'int', 'title': 'Demo Int', 'value': 7})
    status, _, body = request(port, 'PUT', '/admin/option/file',
                              {'file': 'smoke_opt.json', 'data': edited}, cookie=cookie)
    assert json.loads(body)['result'] is True, body
    on_disk = json.loads((target / 'options/smoke_opt.json').read_text(encoding='utf-8-sig'))
    assert len(on_disk['classList'][0]['options']) == 3
    status, _, body = request(port, 'GET', '/admin/option/file?file=global.json', cookie=cookie)
    locked_def = json.loads(body)['data']
    status, _, body = request(port, 'PUT', '/admin/option/file',
                              {'file': 'global.json', 'data': locked_def}, cookie=cookie)
    result = json.loads(body)
    assert result['result'] is False and '锁定' in result['message'], result
    status, _, body = request(port, 'DELETE', '/admin/option/file?file=global.json', cookie=cookie)
    assert json.loads(body)['result'] is False
    status, _, body = request(port, 'POST', '/admin/option/file/menu',
                              {'file': 'smoke_opt.json'}, cookie=cookie)
    assert json.loads(body)['result'] is True, body
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute("SELECT COUNT(*) FROM menu WHERE href = "
                          "'/admin/view/option?file=smoke_opt.json' AND isDelete = 0").fetchone()[0] == 1
    status, _, body = request(port, 'POST', '/admin/option/file/menu',
                              {'file': 'smoke_opt.json'}, cookie=cookie)
    result = json.loads(body)
    assert result['result'] is False and '菜单' in result['message'], result
    status, _, body = request(port, 'GET', '/admin/option/files', cookie=cookie)
    smoke_row = {row['file']: row for row in json.loads(body)['data']}['smoke_opt.json']
    assert smoke_row['inMenu'] is True and smoke_row['menuTitle'] == 'Smoke Opt'
    status, _, body = request(port, 'DELETE', '/admin/option/file?file=smoke_opt.json', cookie=cookie)
    assert json.loads(body)['result'] is True, body
    assert not (target / 'options/smoke_opt.json').exists()
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute("SELECT COUNT(*) FROM menu WHERE href = "
                          "'/admin/view/option?file=smoke_opt.json' AND isDelete = 0").fetchone()[0] == 0
    status, _, body = request(port, 'DELETE', '/admin/option/file?file=smoke_opt.json', cookie=cookie)
    assert json.loads(body)['result'] is False
    print('PASS option file management CRUD, locked guard and menu linkage')
    assert request(port, 'GET', '/api/v1/profile')[0] == 401
    assert request(port, 'GET', '/api/v1/profile', cookie=cookie)[0] == 401
    member = 'smoke_member'
    payload = {'username': member, 'password': PASSWORD, 'nickname': 'Smoke Member'}
    status, _, body = request(port, 'POST', '/api/v1/register', payload)
    assert status == 201 and json.loads(body)['code'] == 0, body
    status, headers, body = request(port, 'POST', '/api/v1/login', {**payload, 'identifier': member})
    assert status == 200 and json.loads(body)['code'] == 0, body
    member_cookie = headers['Set-Cookie'].split(';')[0]
    assert member_cookie.startswith('MSID=')
    status, _, body = request(port, 'GET', '/api/v1/profile', cookie=member_cookie)
    assert status == 200 and json.loads(body)['data']['username'] == member, body
    for path in ('/api/v1/balance', '/api/v1/balance/log'):
        status, _, body = request(port, 'GET', path, cookie=member_cookie)
        assert status == 200 and json.loads(body)['code'] == 0, (path, status, body)
    assert request(port, 'GET', '/admin', cookie=member_cookie)[0] == (404 if protected else 302)
    assert request(port, 'GET', '/api/v1/logout', cookie=member_cookie)[0] == 405  # R1
    assert request(port, 'POST', '/api/v1/logout', cookie=member_cookie)[0] == 200
    assert request(port, 'GET', '/api/v1/profile', cookie=member_cookie)[0] == 401
    print('PASS member registration/login/profile/balance/logout; cookie separation')
    for invalid in ('13800138000', 'name@example.com', 'a name', 'ab', 'a\u0000bc'):
        status, _, body = request(port, 'POST', '/api/v1/register', {
            'username': invalid, 'password': PASSWORD})
        assert status == 400 and json.loads(body)['code'] == 400, (invalid, status, body)
    status, _, body = request(port, 'POST', '/api/v1/register', {
        'username': member.upper(), 'password': PASSWORD})
    assert json.loads(body)['code'] == 409, (status, body)
    for field in ('phone', 'email', 'phone_verified_at', 'email_verified_at'):
        status, _, body = request(port, 'PUT', '/admin/member/user', {'id': 1, field: ''}, cookie=cookie)
        assert status == 400 and json.loads(body)['result'] is False, (field, status, body)
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute('SELECT count(*) FROM xadmin_migration WHERE component=?', ('identity',)).fetchone() == (1,)
    with sqlite3.connect(target / 'db/identity-before-v1.db') as db:
        assert db.execute("SELECT count(*) FROM sqlite_master WHERE name='xadmin_migration'").fetchone() == (0,)
    print('PASS account rules, case-insensitive uniqueness, dedicated-contact boundary and migration backup')
    status, _, body = request(port, 'POST', login_path, {'username': 'smoke_added', 'password': client_hash('smoke_added', PASSWORD)})
    assert status == 200 and not json.loads(body)['result']
    denied = USER + '_denied'
    status, headers, body = request(port, 'POST', login_path, {'username': denied, 'password': client_hash(denied, PASSWORD)})
    assert status == 200 and json.loads(body)['result']
    denied_cookie = headers['Set-Cookie'].split(';')[0]
    assert request(port, 'GET', '/admin/auth/user', cookie=denied_cookie)[0] == 403
    print('PASS deleted-user login and role denial')
    def parallel_read(i):
        status, _, body = request(port, 'GET', '/admin/auth/user?page=1&limit=10', cookie=cookie)
        assert status == 200 and json.loads(body)['data']
    if functional_only:
        parallel_read(0)
        print('PASS shared SQL read (parallel check skipped)')
    else:
        with ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(parallel_read, range(40)))
        print('PASS concurrent reads with shared legacy SQL statements')
    write_checks(port, target, cookie, login_path, request, client_hash, PASSWORD)
    assert request(port, 'GET', '/admin/logout', cookie=cookie)[0] == 404  # R1: POST-only
    status, headers, body = request(port, 'POST', '/admin/logout', cookie=cookie)
    assert status == 302 and 'Max-Age=0' in headers.get('Set-Cookie', ''), (status, headers)
    status, headers, body = request(port, 'GET', '/admin', cookie=cookie)
    assert status == (404 if protected else 302), status
    print('PASS logout invalidates server session')
    status, headers, body = request(port, 'POST', login_path, {'username': USER, 'password': client_hash(USER, PASSWORD)})
    cookie = headers['Set-Cookie'].split(';')[0]
    assert request(port, 'POST', '/__test/expire', cookie=cookie)[0] == 200
    assert request(port, 'GET', '/admin', cookie=cookie)[0] == (404 if protected else 302)
    print('PASS expired session rejected')
    # tool/reload family: v1 page bytes, template rebuild over the fixture tree,
    # and the three submit actions rotating the script generation.
    assert request(port, 'POST', '/admin/tool/reload/template')[0] == (404 if protected else 302)
    status, headers, body = request(port, 'POST', login_path, {'username': USER, 'password': client_hash(USER, PASSWORD)})
    assert json.loads(body)['result'], body
    cookie = headers['Set-Cookie'].split(';')[0]
    status, _, body = request(port, 'GET', '/admin/view/tool/reload', cookie=cookie)
    assert status == 200 and body == (ROOT / 'page/tool/reload.html').read_bytes()
    expected_templates = sum(len(files) for _, _, files in os.walk(ROOT / 'template'))
    for method in ('GET', 'POST'):
        status, _, body = request(port, method, '/admin/tool/reload/template', cookie=cookie)
        result = json.loads(body)
        assert status == 200 and result['result'] and result['data']['total'] == expected_templates, (method, result)
        assert result['data']['loaded'] + result['data']['failed'] == result['data']['total']
    print('PASS tool reload view page and template cache rebuild')
    for action in ('host', 'server', 'xs'):
        status, _, body = request(port, 'POST', '/admin/tool/reload/' + action, cookie=cookie)
        result = json.loads(body)
        assert status == 200 and result['result'] and result['data']['queued'] and result['data']['id'] > 0, (action, result)
        for _ in range(60):
            if request(port, 'GET', '/admin', cookie=cookie)[0] == (404 if protected else 302):
                break
            time.sleep(0.1)
        else:
            raise AssertionError('admin tool reload ' + action + ' did not rotate generation')
        status, headers, body = request(port, 'POST', login_path, {'username': USER, 'password': client_hash(USER, PASSWORD)})
        assert json.loads(body)['result'], (action, body)
        cookie = headers['Set-Cookie'].split(';')[0]
    print('PASS tool reload host/server/xs submit and generation rotation')
    # === attack-review fixes (2026-09-11) ===
    import time as _time
    # F1+F3: banned member loses session; MSID carries SameSite; profile PUT cannot flip status.
    fixed_member = 'fix_member'
    fixed_payload = {'username': fixed_member, 'password': PASSWORD}
    status, _, body = request(port, 'POST', '/api/v1/register', fixed_payload)
    assert json.loads(body)['code'] == 0, body
    status, headers, body = request(port, 'POST', '/api/v1/login', {**fixed_payload, 'identifier': fixed_member})
    assert 'SameSite=Lax' in headers.get('Set-Cookie', ''), headers.get('Set-Cookie')
    fix_cookie = headers['Set-Cookie'].split(';')[0]
    with sqlite3.connect(target / 'db/main.db') as db:
        fix_id = db.execute('SELECT id FROM member WHERE username=?', (fixed_member,)).fetchone()[0]
    status, _, body = request(port, 'PUT', '/admin/member/user', {
        'id': fix_id, 'groupId': 1, 'authLevel': 0, 'nickname': 'n', 'avatar': '', 'status': 0}, cookie=cookie)
    assert json.loads(body)['result'], body
    assert request(port, 'GET', '/api/v1/profile', cookie=fix_cookie)[0] == 401
    status, _, body = request(port, 'POST', '/api/v1/login', {**fixed_payload, 'identifier': fixed_member})
    assert json.loads(body)['code'] != 0, body
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute('SELECT status FROM member WHERE id=?', (fix_id,)).fetchone()[0] == 0
    # F2: masked routes keep no body; unmasked routes keep byte-exact bodies.
    status, _, body = request(port, 'POST', '/admin/member/user', {
        'username': 'fix_masked', 'password': PASSWORD,
        'groupId': 1}, cookie=cookie)
    assert json.loads(body)['result'], body
    with sqlite3.connect(target / 'db/main.db') as db:
        row = db.execute("SELECT body FROM logs WHERE uri='/admin/member/user' "
                         'ORDER BY id DESC LIMIT 1').fetchone()
        assert row == ('',), row
    status, _, body = request(port, 'POST', '/admin/auth/role', {
        'name': 'fix_role', 'desc': 'd', 'authList': '[]', 'authLevel': 0}, cookie=cookie)
    assert json.loads(body)['result'], body
    with sqlite3.connect(target / 'db/main.db') as db:
        row = db.execute("SELECT body FROM logs WHERE uri='/admin/auth/role' "
                         'ORDER BY id DESC LIMIT 1').fetchone()
        assert row and 'fix_role' in row[0], row
    # F7: guarded login attempts are audited with empty body.
    status, headers, _ = request(port, 'POST', login_path, {
        'username': USER, 'password': client_hash(USER, PASSWORD)})
    with sqlite3.connect(target / 'db/main.db') as db:
        rows = db.execute("SELECT body FROM logs WHERE uri = ? "
                          'ORDER BY id DESC LIMIT 1', (login_path,)).fetchall()
        assert rows and rows[0][0] == '', rows
    # F9: list limit capped at 100.
    status, _, body = request(port, 'GET', '/admin/logs?limit=99999&page=1', cookie=cookie)
    assert status == 200 and len(json.loads(body)['data']) <= 100
    # F6: permission rebuild is O(routes+ΣauthList); a write stays fast with 20k roles.
    if not functional_only:
        with sqlite3.connect(target / 'db/main.db') as db:
            db.execute("WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM c WHERE x<20000) "
                       'INSERT INTO role(name,desc,authList,authLevel,createTime,updateTime,isDelete) '
                       "SELECT 'perf_seed_'||x,'','[]',0,?,?,0 FROM c", (legacy_now(), legacy_now()))
            db.commit()
    status, _, body = request(port, 'POST', '/admin/auth/role', {
        'name': 'perf_gate', 'desc': '', 'authList': '[]', 'authLevel': 0}, cookie=cookie)
    gate_id = json.loads(body)['data']['id']
    t0 = _time.perf_counter()
    status, _, body = request(port, 'PUT', '/admin/auth/role', {
        'id': gate_id, 'name': 'perf_gate2', 'desc': '', 'authList': '[1]', 'authLevel': 0}, cookie=cookie)
    elapsed = _time.perf_counter() - t0
    assert json.loads(body)['result'], body[:120]
    if not functional_only:
        assert elapsed < 0.5, (elapsed, body[:120])
    request(port, 'DELETE', '/admin/auth/role?id=' + str(gate_id), cookie=cookie)
    if functional_only:
        print('PASS F1/F2/F3/F6/F7/F9 functional regressions (bulk/timing check skipped)')
    else:
        print('PASS attack fixes F1/F2/F3/F6/F7/F9 (%.0fms at 20k roles)' % (elapsed * 1000))
    # === plugin host: scan / lifecycle / full-ABI conformance (hello-sdk) ===
    status, _, body = request(port, 'GET', '/admin/plugin/list', cookie=cookie)
    result = json.loads(body)
    assert result['code'] == 0 and result['count'] >= 11, (result['count'], result)
    plugins = {row['name']: row for row in result['data']}
    assert 'hello-sdk' in plugins and plugins['hello-sdk']['status'] == 'discovered'
    assert plugins['hello-sdk']['title'] == 'Hello SDK Conformance Plugin'
    status, _, body = request(port, 'GET', '/admin/plugin/get?name=hello-sdk', cookie=cookie)
    assert json.loads(body)['result']
    # enable -> compile + lifecycle + registrations
    status, _, body = request(port, 'POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, cookie=cookie)
    assert json.loads(body)['result'], body
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/ping')
    assert status == 200 and json.loads(body)['result'] and json.loads(body)['message'] == 'hello-sdk ok', body
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/temp')
    assert status == 200 and body == b'temp'
    # event round-trip
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/emit')
    assert json.loads(body)['events'] == 1, body
    # hook chain (self hook, CONTINUE -> 0)
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/hook')
    result = json.loads(body)
    assert result['ret'] == 0 and result['payload'] == 1, result
    # service provide+acquire+release
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/service')
    result = json.loads(body)
    assert result['acquire'] == 0 and result['result'] and result['message'] == 'hello-sdk ok', result
    # unregister token
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/unregister')
    assert json.loads(body)['result'] is True, body
    assert request(port, 'GET', '/api/plugin/hello-sdk/temp')[0] == 404
    # admin-only plugin route: anonymous redirect; 超管经 auto-grant 直达 200
    # （v1 GrantDefaultAdminRoleAuth 语义：新 auth 自动授 role 1，此前实测 403）
    expected_anon = 404 if protected else 302
    assert request(port, 'GET', '/api/plugin/hello-sdk/admin-echo')[0] == expected_anon
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/admin-echo', cookie=cookie)
    assert status == 200 and json.loads(body)['result'], (status, body[:100])
    # settings: config change round-trip via OnConfigChanged
    status, _, body = request(port, 'POST', '/admin/plugin/settings', {
        'name': 'hello-sdk', 'config': {'welcomeMessage': 'changed-by-smoke', 'showTime': False}}, cookie=cookie)
    assert json.loads(body)['result'], body
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/ping')
    assert json.loads(body)['message'] == 'changed-by-smoke', body
    # DB ledger: menu/auth/uris ownership + generation row
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute("SELECT COUNT(*) FROM menu WHERE plugin_xid='hello-sdk' AND isDelete=0").fetchone()[0] == 1
        assert db.execute("SELECT COUNT(*) FROM auth WHERE plugin_xid='hello-sdk' AND isDelete=0").fetchone()[0] == 1
        assert db.execute("SELECT COUNT(*) FROM authGroup WHERE plugin_xid='hello-sdk' AND isDelete=0").fetchone()[0] == 1
        assert db.execute("SELECT COUNT(*) FROM uris WHERE plugin_xid='hello-sdk'").fetchone()[0] == 2
        gen, state = db.execute("SELECT generation, state FROM plugin_generation WHERE xid='hello-sdk' ORDER BY id DESC LIMIT 1").fetchone()
        assert state == 'active' and gen == 1
        assert db.execute("SELECT COUNT(*) FROM plugin_resource WHERE xid='hello-sdk' AND status='active'").fetchone()[0] >= 10
    # reload -> new generation, counters reset, routes alive
    status, _, body = request(port, 'POST', '/admin/plugin/reload', {'name': 'hello-sdk'}, cookie=cookie)
    assert json.loads(body)['result'], body
    status, _, body = request(port, 'GET', '/api/plugin/hello-sdk/state')
    # reload = 全新插件镜像：插件内静态计数器归零，配置跨代保留
    state = json.loads(body)
    assert state['starts'] == 1 and state['message'] == 'changed-by-smoke', state
    with sqlite3.connect(target / 'db/main.db') as db:
        gen = db.execute("SELECT MAX(generation) FROM plugin_generation WHERE xid='hello-sdk'").fetchone()[0]
        assert gen == 2, gen
    # 注册幂等 upsert：reload 后 menu/authGroup/auth/uris 不累积重复行
    with sqlite3.connect(target / 'db/main.db') as db:
        q = lambda sql: db.execute(sql).fetchone()[0]
        assert q("SELECT COUNT(*) FROM menu WHERE plugin_xid='hello-sdk'") == 1, 'menu accumulated'
        assert q("SELECT COUNT(*) FROM authGroup WHERE plugin_xid='hello-sdk'") == 1, 'authGroup accumulated'
        assert q("SELECT COUNT(*) FROM auth WHERE plugin_xid='hello-sdk'") == 1, 'auth accumulated'
        assert q("SELECT COUNT(*) FROM uris WHERE plugin_xid='hello-sdk'") == 2, 'uris accumulated'
    print('PASS plugin registration idempotent upsert across reload')
    # disable -> teardown: routes gone, resources reclaimed, runtime disabled
    status, _, body = request(port, 'POST', '/admin/plugin/disable', {'name': 'hello-sdk'}, cookie=cookie)
    assert json.loads(body)['result'], body
    assert request(port, 'GET', '/api/plugin/hello-sdk/ping')[0] == 404
    with sqlite3.connect(target / 'db/main.db') as db:
        assert db.execute("SELECT COUNT(*) FROM menu WHERE plugin_xid='hello-sdk' AND isDelete=0").fetchone()[0] == 0
        assert db.execute("SELECT COUNT(*) FROM uris WHERE plugin_xid='hello-sdk'").fetchone()[0] == 0
        assert db.execute("SELECT COUNT(*) FROM plugin_resource WHERE xid='hello-sdk' AND status='active'").fetchone()[0] == 0
        enabled, status_ = db.execute("SELECT enabled, status FROM plugin_runtime WHERE xid='hello-sdk'").fetchone()
        assert enabled == 0 and status_ == 'disabled'
        state = db.execute("SELECT state FROM plugin_generation WHERE xid='hello-sdk' ORDER BY id DESC LIMIT 1").fetchone()[0]
        assert state == 'stopped'
    print('PASS plugin host scan/enable/lifecycle/resources/reload/disable (hello-sdk)')
    # R1/R3/R4: logout method, repwd revocation, per-account session cap.
    r_member = 'r34_member'
    r_payload = {'username': r_member, 'password': PASSWORD}
    status, _, body = request(port, 'POST', '/api/v1/register', r_payload)
    assert json.loads(body)['code'] == 0, body
    cookies = []
    for _ in range(6):
        status, headers, body = request(port, 'POST', '/api/v1/login', {**r_payload, 'identifier': r_member})
        assert json.loads(body)['code'] == 0, body
        cookies.append(headers['Set-Cookie'].split(';')[0])
    # R4: cap 5 -> 第 1 个（最旧）被踢，第 6 个存活
    assert request(port, 'GET', '/api/v1/profile', cookie=cookies[0])[0] == 401, 'oldest session not evicted'
    assert request(port, 'GET', '/api/v1/profile', cookie=cookies[5])[0] == 200
    # R3: 管理员重置密码 -> 该账号全部会话撤销；当前会话自助改密 -> 保留自己
    with sqlite3.connect(target / 'db/main.db') as db:
        r_id = db.execute('SELECT id FROM member WHERE username=?', (r_member,)).fetchone()[0]
    status, _, body = request(port, 'POST', '/admin/member/user/repwd', {
        'id': r_id, 'username': r_member, 'password': r_payload['password']}, cookie=cookie)
    assert json.loads(body)['result'], body
    assert request(port, 'GET', '/api/v1/profile', cookie=cookies[5])[0] == 401, 'repwd kept stale sessions'
    status, headers, body = request(port, 'POST', '/api/v1/login', {**r_payload, 'identifier': r_member})
    own = headers['Set-Cookie'].split(';')[0]
    new_hash = PASSWORD + '-x'
    status, _, body = request(port, 'POST', '/api/v1/profile/password', {
        'oldPassword': r_payload['password'], 'newPassword': new_hash}, cookie=own)
    assert json.loads(body)['code'] == 0, body
    assert request(port, 'GET', '/api/v1/profile', cookie=own)[0] == 200, 'current session dropped'
    status, _, body = request(port, 'POST', '/api/v1/login', {
        'identifier': r_member, 'password': new_hash})
    assert json.loads(body)['code'] == 0, body
    print('PASS logout POST-only, repwd revocation and session cap (R1/R3/R4)')
    # F4+F5: realm-split guard; member lockout leaves admin login alone (run last).
    for i in range(6):
        request(port, 'POST', '/api/v1/login', {
            'identifier': member, 'password': 'wrong-password-' + str(i)})
    status, _, body = request(port, 'POST', login_path, {
        'username': USER, 'password': client_hash(USER, PASSWORD)})
    assert json.loads(body)['result'], body
    status, _, body = request(port, 'POST', '/api/v1/login', {
        'identifier': member, 'password': PASSWORD})
    result = json.loads(body)
    assert status == 429 and result['code'] == 429 and result['msg'], result
    print('PASS guard realm split and readable lockout message (F4/F5)')
    status, headers, _ = request(port, 'POST', login_path, {'username': USER, 'password': client_hash(USER, PASSWORD)})
    cookie = headers['Set-Cookie'].split(';')[0]
    assert request(port, 'GET', '/admin', cookie=cookie)[0] == 200
    status, _, body = request(port, 'POST', '/__test/reload')
    assert status == 202 and json.loads(body)['id'] > 0
    for _ in range(60):
        status, _, _ = request(port, 'GET', '/admin', cookie=cookie)
        if status == (404 if protected else 302):
            break
        time.sleep(0.1)
    else:
        raise AssertionError('reload did not rotate script/session generation')
    assert request(port, 'GET', login_path)[0] == 200
    # xs finalizes the old generation after its final request releases its lease.
    for _ in range(30):
        if 'Option_Unit' in (target / 'server.log').read_text(encoding='utf-8', errors='replace'):
            break
        time.sleep(0.1)
    else:
        raise AssertionError('old generation cleanup not observed')
    print('PASS script reload, old-generation cleanup, session invalidation')



def register_rate_check(port, executable=None):
    """R2: 默认配置（60s/IP）下第二次注册被拒；主夹具把间隔设 0 绕过。"""
    target = fixture(port, register_interval=None)
    log = open(target / 'server.log', 'ab')
    process = subprocess.Popen([str(executable or DEFAULT_EXECUTABLE), str(target / 'xs.json')], cwd=ROOT,
                               stdout=log, stderr=subprocess.STDOUT,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    try:
        for _ in range(60):
            if process.poll() is not None:
                raise RuntimeError('rate fixture exited')
            try:
                if request(port, 'GET', '/admin/login')[0] in (200, 404):
                    break
            except OSError:
                pass
            time.sleep(0.3)
        user = 'rate_member'
        payload = {'username': user, 'password': PASSWORD}
        status, _, body = request(port, 'POST', '/api/v1/register', payload)
        assert json.loads(body)['code'] == 0, body
        status, _, body = request(port, 'POST', '/api/v1/register', {
            'username': user + '2', 'password': PASSWORD})
        result = json.loads(body)
        assert result['code'] == 429, result
        print('PASS register rate limit 1/min/IP (R2)')
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
        log.close()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19081)
    parser.add_argument('--keep-running', action='store_true')
    parser.add_argument('--protected-entry', action='store_true')
    parser.add_argument('--exe', type=Path, default=DEFAULT_EXECUTABLE,
                        help='validate a candidate xs without replacing the workspace executable')
    parser.add_argument('--functional-only', action='store_true',
                        help='skip parallel requests and bulk/timing checks; retain functional regressions')
    args = parser.parse_args()
    verify_assets()
    db_hash = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', args.port))
    target = fixture(args.port, args.protected_entry)
    log = target / 'server.log'
    with log.open('wb') as output:
        process = subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    passed = False
    rate_port = None
    try:
        for _ in range(60):
            if process.poll() is not None:
                raise RuntimeError('xs exited before readiness')
            try:
                status, _, body = request(args.port, 'GET', '/smoke-private-entry' if args.protected_entry else '/admin/login')
                if status == 200:
                    break
                if b'compilation' in body.lower() or status == 503:
                    raise RuntimeError(f'xs returned {status}: {body[:200]!r}')
            except (ConnectionError, TimeoutError, OSError):
                pass
            time.sleep(0.2)
        else:
            raise RuntimeError('xs did not become ready')
        checks(args.port, target, args.protected_entry, args.functional_only)
        rate_port = args.port + 1
        register_rate_check(rate_port, args.exe.resolve())
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == db_hash
        print('PASS migrated root database unchanged by tests')
        passed = True
    finally:
        if not (passed and args.keep_running):
            process.terminate()
            process.wait(timeout=10)
        print('Fixture:', target)
        print('Server PID:', process.pid, '(running)' if process.poll() is None else '(stopped)')
        if not passed:
            print(log.read_text(encoding='utf-8', errors='replace')[-14000:])


if __name__ == '__main__':
    main()
