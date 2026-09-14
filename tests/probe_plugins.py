"""快速编译加载插件验证（可复用，支持任意 xid 列表）"""
import sys, subprocess, time, json, http.client, shutil
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import smoke

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 19240
XIDS = sys.argv[2:] if len(sys.argv) > 2 else ['guestbook_v3', 'xlogserver']

target = smoke.fixture(PORT)
for p in XIDS:
    dst = target / 'plugin' / p
    if dst.exists(): shutil.rmtree(dst)
    src = Path(__file__).parent / 'plugins' / p
    if src.exists():
        shutil.copytree(src, dst)
log = open(target / 'probe.log', 'wb')
proc = subprocess.Popen([str(Path(__file__).parents[1] / 'xs.exe'), str(target / 'xs.json')],
                        stdout=log, stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
time.sleep(6)

def req(method, path, data=None, cookie=None):
    c = http.client.HTTPConnection('127.0.0.1', PORT, timeout=30)
    h = {}
    if data is not None:
        data = json.dumps(data).encode() if not isinstance(data, bytes) else data
        h['Content-Type'] = 'application/json'
    if cookie: h['Cookie'] = cookie
    c.request(method, path, data, h)
    r = c.getresponse(); b = r.read(); c.close()
    return r.status, b[:200]

c = http.client.HTTPConnection('127.0.0.1', PORT, timeout=15)
c.request('POST', '/admin/login', json.dumps({'username': 'migration_smoke',
    'password': smoke.client_hash('migration_smoke', 'Temporary-test-only-9081')}).encode(),
    {'Content-Type': 'application/json'})
r = c.getresponse(); r.read(); ck = r.getheader('Set-Cookie').split(';')[0]; c.close()

for xid in XIDS:
    s, b = req('POST', '/admin/plugin/enable', {'name': xid}, ck)
    print(f'enable {xid}: {s} {b[:80]}')
print('alive:', proc.poll() is None)
time.sleep(0.5)
proc.terminate()
try: proc.wait(timeout=5)
except: proc.kill()
log.close()
txt = open(target / 'probe.log', errors='replace').read()
for l in txt.splitlines():
    low = l.lower()
    if any(x in low for x in [x.lower() for x in XIDS]) or ('error' in low and 'template' not in low):
        if 'warning' not in low:
            print(' ', l[:180])
