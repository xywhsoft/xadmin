# -*- coding: utf-8 -*-
"""扩展库门禁 e2e：
正例——完整 xs（sqlite+xsmtp+md4c）正常启动；
负例——精简 xs（仅 sqlite）拒启：日志报缺失扩展 + 全站 503 + 无 ready。"""
import sys, time, subprocess, json
sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from pathlib import Path
import importlib.util
spec = importlib.util.spec_from_file_location('smoke', Path(__file__).parent / 'smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)

ROOT = smoke.ROOT
ok = fail = 0
def check(name, cond, detail=''):
    global ok, fail
    if cond: ok += 1; print('PASS', name)
    else: fail += 1; print('FAIL', name, '|', detail)

STRIPPED = ROOT / 'tests/.runtime/xs-stripped.exe'
if not STRIPPED.exists():
    print('SKIP negative (stripped exe not built)')
    sys.exit(0)

# ---- 负例：精简 xs 拒启（sqlite+xsmtp，缺 md4c——无头依赖，走运行期探测）----
# 注：缺 xsmtp 的构建会在编译期失败（mail.h 找不到 xsmtp.h），到不了运行期探测；
# md4c 经手工声明消费，是运行期探测的典型场景。
PORT = 18263
target = smoke.fixture(PORT)
proc = subprocess.Popen([str(STRIPPED), str(target / 'xs.json')], cwd=ROOT,
                        stdout=open(target / 'server.log', 'ab'), stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
try:
    time.sleep(2.0)
    alive = proc.poll() is None
    log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
    check('server process alive (拒绝启动但进程存活)', alive, 'poll=%s' % proc.poll())
    check('missing extension reported (md4c)', 'required xs extension missing: md4c' in log, log[-400:])
    check('no [xadmin] ready line', '[xadmin] ready' not in log, '')
    st, _, body = smoke.request(PORT, 'GET', '/admin/login')
    check('all requests 503', st == 503, 'status=%d' % st)
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.5)

# ---- 正例：完整 xs 正常启动（同份业务代码） ----
PORT = 18264
target = smoke.fixture(PORT)
proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                        stdout=open(target / 'server.log', 'ab'), stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
try:
    for _ in range(80):
        if proc.poll() is not None: raise RuntimeError('server exited')
        try:
            if smoke.request(PORT, 'GET', '/admin/login')[0] in (200, 404): break
        except OSError: pass
        time.sleep(0.3)
    st, _, body = smoke.request(PORT, 'GET', '/admin/login')
    check('full xs boots fine (no 503)', st in (200, 404), 'status=%d' % st)
    log = (target / 'server.log').read_text(encoding='utf-8', errors='replace')
    check('ready line present', '[xadmin] ready' in log, '')
    check('no missing-extension error', 'required xs extension missing' not in log, '')
finally:
    proc.terminate()
    try: proc.wait(5)
    except Exception: proc.kill()
    time.sleep(0.3)

print('=' * 60)
print('RESULT: %d pass / %d fail' % (ok, fail))
sys.exit(1 if fail else 0)
