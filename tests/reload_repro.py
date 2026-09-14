# -*- coding: utf-8 -*-
"""聚焦复现：高压流量下的密集主脚本重载（捕捉静默崩溃与退出码）。"""
import sys, time, subprocess, threading, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import smoke
from campaign_10h import Client, admin_cookie, member_cookie, multipart_body, jload

PORT = 19610
RELOAD_EVERY = 20  # 秒
DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 1800

def main():
    target = smoke.fixture(PORT)
    log = open(target / 'repro.log', 'wb')
    proc = subprocess.Popen([str(smoke.ROOT / 'xs.exe'), str(target / 'xs.json')],
                            cwd=smoke.ROOT, stdout=log, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    c = Client(PORT, timeout=30)
    for _ in range(120):
        if proc.poll() is not None:
            raise SystemExit('died at boot')
        try:
            if c.request('GET', '/admin/login')[0] == 200: break
        except OSError: pass
        time.sleep(0.25)
    import sqlite3
    with sqlite3.connect(target / 'db/main.db') as db:
        db.execute('UPDATE sched_task SET enabled = 0')
        db.commit()
    ck = admin_cookie(c)
    mck = member_cookie(c, 'repro_m')
    up = multipart_body({'modelName': 'r'}, 'file', 'r.zip', b'R' * 1024)
    s, _, b_, _ = c.request('POST', '/admin/attachment/upload', up[1], ck, up[0])
    xid = jload(b_)['data']['xid']

    stop = threading.Event()
    errs = {'n': 0, 'req': 0}
    def hammer(i):
        cc = Client(PORT, timeout=20)
        while not stop.is_set():
            try:
                paths = ['/admin/auth/user?page=1&limit=10', '/api/v1/notify/unread_count',
                         '/attachment?xid=' + xid, '/admin/sched/dashboard']
                s, _, _, _ = cc.request('GET', paths[i % 4], None, ck if i % 4 == 0 or i % 4 == 3 else mck)
                errs['req'] += 1
                if s >= 500 or s == 0:
                    errs['n'] += 1
            except OSError:
                errs['n'] += 1
                time.sleep(0.05)
    threads = [threading.Thread(target=hammer, args=(i,), daemon=True) for i in range(14)]
    for t in threads: t.start()

    reloads = 0
    t0 = time.time()
    while time.time() - t0 < DURATION:
        time.sleep(RELOAD_EVERY)
        if proc.poll() is not None:
            break
        try:
            rc = Client(PORT, timeout=15).request('POST', '/__test/expire', None, ck)
            reloads += 1
        except OSError:
            pass
        if proc.poll() is not None:
            break
    stop.set()
    code = proc.poll()
    if code is None:
        proc.terminate()
        proc.wait(timeout=15)
        print('SURVIVED %d reloads, %d req, %d errs' % (reloads, errs['req'], errs['n']))
    else:
        print('CRASHED after %d reloads, %d req, %d errs, exit code %s' % (reloads, errs['req'], errs['n'], code))
        tail = open(target / 'repro.log', errors='replace').read()[-600:]
        print('log tail:', tail)

if __name__ == '__main__':
    main()
