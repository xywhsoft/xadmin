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
            assert hashlib.sha256((ROOT / entry[key]).read_bytes()).hexdigest() == entry['sha256'], entry[key]
    print('PASS', len(entries), 'unchanged v1 UI/plugin assets')


def fixture(port, protected=False):
    base = ROOT / 'tests/.runtime'
    base.mkdir(parents=True, exist_ok=True)
    target = Path(tempfile.mkdtemp(prefix='smoke-', dir=base))
    for name in ('wwwroot', 'page', 'template', 'options'):
        shutil.copytree(ROOT / name, target / name)
    # The source may have a concealed admin entry. Only the disposable fixture
    # disables it; dedicated tests cover the protected-entry setting separately.
    global_path = target / 'options/global.json'
    options = json.loads(global_path.read_text(encoding='utf-8-sig'))
    for group in options.get('classList', []):
        for option in group.get('options', []):
            if option.get('name') == 'cp_url':
                option['value'] = '/smoke-private-entry' if protected else ''
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
    config = json.loads((ROOT / 'xs.json').read_text(encoding='utf-8'))
    service = config['services'][0]
    service['port'] = port
    host = service['host_default']
    host['devfile'] = str(ROOT / 'tests/host.c')
    host['dev_inc'] = str(ROOT / 'includes')
    host['dev_lib'] = str(ROOT / 'librarys')
    (target / 'xs.json').write_text(json.dumps(config), encoding='utf-8')
    return target


def request(port, method, path, data=None, cookie=None):
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    headers = {}
    if data is not None:
        data = json.dumps(data).encode() if not isinstance(data, bytes) else data
        headers['Content-Type'] = 'application/json'
    if cookie:
        headers['Cookie'] = cookie
    try:
        conn.request(method, path, data, headers)
        resp = conn.getresponse()
        return resp.status, dict(resp.getheaders()), resp.read()
    finally:
        conn.close()


def checks(port, target, protected=False):
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
        assert abs(now - legacy_now()) < 10, now
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
    assert request(port, 'GET', '/api/v1/profile')[0] == 401
    assert request(port, 'GET', '/api/v1/profile', cookie=cookie)[0] == 401
    member = 'smoke_member'
    payload = {'username': member, 'password': client_hash(member, PASSWORD), 'nickname': 'Smoke Member'}
    status, _, body = request(port, 'POST', '/api/v1/register', payload)
    assert status == 200 and json.loads(body)['code'] == 0, body
    status, headers, body = request(port, 'POST', '/api/v1/login', payload)
    assert status == 200 and json.loads(body)['code'] == 0, body
    member_cookie = headers['Set-Cookie'].split(';')[0]
    assert member_cookie.startswith('MSID=')
    status, _, body = request(port, 'GET', '/api/v1/profile', cookie=member_cookie)
    assert status == 200 and json.loads(body)['data']['username'] == member, body
    for path in ('/api/v1/balance', '/api/v1/balance/log'):
        status, _, body = request(port, 'GET', path, cookie=member_cookie)
        assert status == 200 and json.loads(body)['code'] == 0, (path, status, body)
    assert request(port, 'GET', '/admin', cookie=member_cookie)[0] == (404 if protected else 302)
    assert request(port, 'POST', '/api/v1/logout', cookie=member_cookie)[0] == 200
    assert request(port, 'GET', '/api/v1/profile', cookie=member_cookie)[0] == 401
    print('PASS member registration/login/profile/balance/logout; cookie separation')
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
    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(parallel_read, range(40)))
    print('PASS concurrent reads with shared legacy SQL statements')
    write_checks(port, target, cookie, login_path, request, client_hash, PASSWORD)
    status, headers, body = request(port, 'GET', '/admin/logout', cookie=cookie)
    assert status == 302 and 'Max-Age=0' in headers.get('Set-Cookie', ''), (status, headers)
    status, headers, body = request(port, 'GET', '/admin', cookie=cookie)
    assert status == (404 if protected else 302), status
    print('PASS logout invalidates server session')
    status, headers, body = request(port, 'POST', login_path, {'username': USER, 'password': client_hash(USER, PASSWORD)})
    cookie = headers['Set-Cookie'].split(';')[0]
    assert request(port, 'POST', '/__test/expire', cookie=cookie)[0] == 200
    assert request(port, 'GET', '/admin', cookie=cookie)[0] == (404 if protected else 302)
    print('PASS expired session rejected')
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19081)
    parser.add_argument('--keep-running', action='store_true')
    parser.add_argument('--protected-entry', action='store_true')
    args = parser.parse_args()
    verify_assets()
    db_hash = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', args.port))
    target = fixture(args.port, args.protected_entry)
    log = target / 'server.log'
    with log.open('wb') as output:
        process = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    passed = False
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
        checks(args.port, target, args.protected_entry)
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
