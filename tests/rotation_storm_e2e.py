# -*- coding: utf-8 -*-
"""轮换风暴门禁：xlogserver 自启（复现触发条件）+ tool reload 三连 + 脚本重载 + 紧轮询。
固化 2026-09-15 竞态类挂死的回归探针——历史上曾必现于 smoke 尾段（多代重载 + 插件自启）。"""
import sys, time, subprocess, json, sqlite3, socket
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

PORT = 18290
target = smoke.fixture(PORT)
db = sqlite3.connect(target / 'db/main.db')
db.execute("UPDATE plugin_runtime SET enabled=1 WHERE xid='xlogserver'")
db.commit(); db.close()
proc = subprocess.Popen([str(smoke.ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=smoke.ROOT,
                        stdout=open(target / 'server.log', 'wb'), stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
ok = fail = 0
def check(name, cond, detail=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '|', detail)

def req(path, method='GET', payload=None, cookie=None, timeout=15):
    conn = socket.create_connection(('127.0.0.1', PORT), timeout=timeout)
    body = json.dumps(payload).encode() if payload is not None else b''
    head = '%s %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\nContent-Type: application/json\r\n' % (method, path)
    if cookie: head += 'Cookie: %s\r\n' % cookie
    head += 'Content-Length: %d\r\n\r\n' % len(body)
    try:
        conn.sendall(head.encode() + body)
        data = b''
        while True:
            c = conn.recv(65536)
            if not c: break
            data += c
    except socket.timeout:
        conn.close()
        return None
    conn.close()
    return data

def relogin():
    r = req('/admin/login', 'POST', {'username': smoke.USER, 'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)}, timeout=10)
    if r is None:
        return None
    for line in r.split(b'\r\n'):
        if line.lower().startswith(b'set-cookie:'):
            return line.split(b':', 1)[1].strip().split(b';')[0].decode()
    return None

try:
    for _ in range(80):
        if proc.poll() is not None:
            raise RuntimeError((target / 'server.log').read_text(encoding='utf-8', errors='replace')[-1500:])
        r = req('/admin/login')
        if r and b'200' in r.split(b'\r\n')[0]:
            break
        time.sleep(0.3)
    cookie = relogin()
    log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
    check('boot with xlogserver autostart', cookie and '[xlogserver] started' in log)

    rounds = [('host', '/admin/tool/reload/host'), ('server', '/admin/tool/reload/server'),
              ('xs', '/admin/tool/reload/xs'), ('final', '/__test/reload'),
              ('again1', '/__test/reload'), ('again2', '/__test/reload')]
    for label, path in rounds:
        r = req(path, 'POST', None, cookie, timeout=15)
        check('%s submit ok' % label, r is not None and b' 200 ' in r.split(b'\r\n')[0] + b' ' or (r is not None and b' 202 ' in r.split(b'\r\n')[0]),
              'resp=%s' % (r.split(b'\r\n')[0] if r else 'HANG'))
        if r is None:
            break
        # 紧轮询轮换（smoke 节奏）
        rotated = False
        last = b''
        for _ in range(120):
            p = req('/admin', 'GET', None, cookie, timeout=5)
            last = p.split(b'\r\n')[0] if p else b'HANG'
            if p and (b' 302' in last or b' 404' in last):
                rotated = True
                break
            time.sleep(0.05)
        check('%s rotated (12s)' % label, rotated, 'last=%s' % last)
        if not rotated:
            break
        cookie = relogin()
        check('%s relogin' % label, cookie is not None)
        if cookie is None:
            break
        r = req('/admin/api/plugin/xlogserver/services', 'GET', None, cookie, timeout=12)
        check('%s xlog services alive' % label, r is not None and b' 200 ' in r.split(b'\r\n')[0] + b' ',
              'resp=%s' % (r.split(b'\r\n')[0] if r else 'HANG'))
        time.sleep(0.1)
    log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
    check('no unexpected errors', '[xadmin][error]' not in log, log[-300:])
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

print('=' * 60)
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
