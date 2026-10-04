# -*- coding: utf-8 -*-
"""安装向导 e2e：全新部署（无 db、无 lock）→ 向导接管 → POST 建库建超管 → 登录成功。
负例：非法用户名/非十六进制哈希拒绝；存量部署（db 在、lock 缺）不进向导。"""
import sys, time, subprocess, json, hashlib, sqlite3, shutil, os, argparse
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
parser.add_argument('--port', type=int, default=18240)
args = parser.parse_args()
original_db = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
ok = fail = 0
def check(name, cond, detail=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '|', detail)

def boot(port, target):
    with (target / 'server.log').open('ab') as output:
        return subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                                stdout=output, stderr=subprocess.STDOUT,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

def wait_up(proc, port, target):
    for _ in range(80):
        if proc.poll() is not None:
            raise RuntimeError('server exited early:\n' + (target / 'server.log').read_text(encoding='utf-8', errors='replace')[-1500:])
        try:
            if smoke.request(port, 'GET', '/')[0] in (200, 404, 302, 503):
                return
        except OSError:
            pass
        time.sleep(0.3)
    raise RuntimeError('server never came up')

ADMIN = 'installer1'
PWD = 'Install#2026'

# ---------- 夹具一：全新部署 ----------
PORT = args.port
target = smoke.fixture(PORT)
import gc, os
gc.collect()
for _ in range(20):   # fixture 的 sqlite 连接靠 GC 释放，Windows 下需等句柄归还
    try:
        (target / 'db/main.db').unlink()
        break
    except PermissionError:
        time.sleep(0.3)   # 全新：无库、无 lock
proc = boot(PORT, target)
try:
    wait_up(proc, PORT, target)
    # 向导接管：任意路径 GET 都是 install.html
    st, _, body = smoke.request(PORT, 'GET', '/')
    check('wizard takes over /', st == 200 and b'install' in body.lower(), 'status=%d' % st)
    st, _, body = smoke.request(PORT, 'GET', '/admin/login')
    check('wizard takes over /admin/login', st == 200 and b'install' in body.lower(), 'status=%d' % st)
    # 负例：非法用户名 / 非 64hex
    st, _, body = smoke.request(PORT, 'POST', '/', {'username': 'x!', 'password': 'a' * 64})
    check('bad username rejected', st == 400 and json.loads(body)['result'] is False, body[:150])
    st, _, body = smoke.request(PORT, 'POST', '/', {'username': ADMIN, 'password': 'zz'})
    check('bad hash rejected', st == 400 and json.loads(body)['result'] is False, body[:150])
    # 正例：安装（客户端哈希 = sha256(user + "_xywhsoft_" + pwd)）
    chash = hashlib.sha256((ADMIN + '_xywhsoft_' + PWD).encode()).hexdigest()
    st, _, body = smoke.request(PORT, 'POST', '/', {'username': ADMIN, 'password': chash})
    check('install succeeds', st == 200 and json.loads(body)['result'] is True, body[:200])
    check('install.lock written', (target / 'install.lock').exists())
    # 安装后立即可登录
    st, hd, body = smoke.request(PORT, 'POST', '/admin/login',
                                 {'username': ADMIN, 'password': chash})
    check('admin login after install', st == 200 and 'Set-Cookie' in hd, 'status=%d body=%s' % (st, body[:150]))
    cookie = hd['Set-Cookie'].split(';')[0]
    st, _, _ = smoke.request(PORT, 'GET', '/admin', cookie=cookie)
    check('admin dashboard reachable', st == 200, 'status=%d' % st)
    # 超管落库形态
    with sqlite3.connect(target / 'db/main.db') as db:
        row = db.execute('SELECT role, authLevel, isDelete FROM user WHERE user=?', (ADMIN,)).fetchone()
    check('admin row role=1 authLevel=999', row == (1, 999, 0), str(row))
    # New installations exercise the same identity migrations and account API,
    # rather than relying only on a migrated copy of the developer database.
    st, _, body = smoke.request(PORT, 'POST', '/api/v1/register', {'username':'fresh_member','password':PWD})
    check('fresh member registers with raw password', st == 201 and json.loads(body)['code'] == 0, body[:150])
    st, hd, body = smoke.request(PORT, 'POST', '/api/v1/login', {'identifier':'fresh_member','password':PWD})
    check('fresh member login issues JWT and cookie', st == 200 and 'access_token' in json.loads(body).get('data',{}), body[:150])
    member_cookie=hd.get('Set-Cookie','').split(';')[0]
    st, _, body = smoke.request(PORT, 'GET', '/api/v1/profile', cookie=member_cookie)
    check('fresh member can read own profile', st == 200 and json.loads(body).get('data',{}).get('username') == 'fresh_member', body[:150])
    st, _, body = smoke.request(PORT, 'GET', '/api/v1/auth/providers')
    check('unconfigured third parties remain disabled', st == 200 and json.loads(body)['data']['providers'] == [], body[:150])
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

# ---------- 夹具二：存量部署（db 在、lock 缺）不进向导 ----------
PORT = args.port + 1
target = smoke.fixture(PORT)   # db 在、无 lock
proc = boot(PORT, target)
try:
    wait_up(proc, PORT, target)
    st, _, body = smoke.request(PORT, 'GET', '/admin/login')
    check('existing db skips wizard', st in (200, 404) and 'xAdmin 安装'.encode() not in body, 'status=%d' % st)
    st, hd, _ = smoke.request(PORT, 'POST', '/admin/login',
                              {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
    check('existing deployment logs in normally', st == 200 and 'Set-Cookie' in hd, 'status=%d' % st)
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.3)

check('real database unchanged', hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original_db)
print('=' * 60)
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
