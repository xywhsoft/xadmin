# -*- coding: utf-8 -*-
"""崩溃监视长浸泡：D-only 混合负载 + 周期重载；进程死亡即抓退出码并核对
WER 转储是否生成（HKCU LocalDumps xs.exe → tests/.runtime/dumps）。
用法: python tests/crash_watch.py [小时数]"""
import ctypes
import json
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke
from campaign_10h import Client, admin_cookie, member_cookie, multipart_body, jload

PORT = 19710
HOURS = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
RUNTIME = ROOT = Path(__file__).resolve().parents[1]
DUMPS = ROOT / 'tests' / '.runtime' / 'dumps'
STATE = ROOT / 'tests' / '.runtime' / 'crash_watch_state.json'

kernel32 = ctypes.windll.kernel32


def exit_code(h):
    code = ctypes.c_ulong()
    if kernel32.GetExitCodeProcess(h, ctypes.byref(code)):
        return code.value
    return -1


def main():
    DUMPS.mkdir(parents=True, exist_ok=True)
    target = smoke.fixture(PORT)
    log = open(target / 'watch.log', 'wb')
    proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')],
                            cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    h = kernel32.OpenProcess(0x1F0FFF, False, proc.pid)  # PROCESS_ALL_ACCESS
    c = Client(PORT, timeout=30)
    for _ in range(120):
        if proc.poll() is not None:
            raise SystemExit('died at boot')
        try:
            if c.request('GET', '/admin/login')[0] == 200:
                break
        except OSError:
            pass
        time.sleep(0.25)
    import sqlite3
    with sqlite3.connect(target / 'db/main.db') as db:
        db.execute('UPDATE sched_task SET enabled = 0')
        db.commit()
    ck = admin_cookie(c)
    mck = member_cookie(c, 'watch_m')
    up = multipart_body({'modelName': 'w'}, 'file', 'w.zip', b'W' * 1024)
    s, _, b_, _ = c.request('POST', '/admin/attachment/upload', up[1], ck, up[0])
    xid = jload(b_)['data']['xid']

    stop = threading.Event()
    stat = {'req': 0, 'err': 0, 'reloads': 0, 'crashed': None}

    def hammer(i):
        cc = Client(PORT, timeout=20)
        paths = ['/admin/auth/user?page=1&limit=10', '/api/v1/notify/unread_count',
                 '/attachment?xid=' + xid, '/admin/sched/dashboard']
        while not stop.is_set():
            try:
                s, _, _, _ = cc.request('GET', paths[i % 4], None,
                                        ck if i % 4 in (0, 3) else mck)
                stat['req'] += 1
                if s >= 500 or s == 0:
                    stat['err'] += 1
            except OSError:
                stat['err'] += 1
                time.sleep(0.05)

    threads = [threading.Thread(target=hammer, args=(i,), daemon=True) for i in range(14)]
    for t in threads:
        t.start()

    t0 = time.time()
    last_reload = 0
    while time.time() - t0 < HOURS * 3600:
        time.sleep(10)
        if proc.poll() is not None:
            break
        # 每 10 分钟重载一次
        if time.time() - last_reload > 600:
            last_reload = time.time()
            try:
                Client(PORT, timeout=15).request('POST', '/__test/expire', None, ck)
                stat['reloads'] += 1
            except OSError:
                pass
        STATE.write_text(json.dumps({
            'minutes': round((time.time() - t0) / 60, 1),
            'req': stat['req'], 'err': stat['err'],
            'reloads': stat['reloads'], 'alive': proc.poll() is None}, ensure_ascii=False))
        if proc.poll() is not None:
            break

    stop.set()
    if proc.poll() is not None:
        code = exit_code(h)
        time.sleep(8)  # 等 WER 落盘
        dumps = sorted(DUMPS.glob('*.dmp'), key=lambda p: p.stat().st_mtime, reverse=True)
        stat['crashed'] = {'exit_code': hex(code), 'minutes': round((time.time() - t0) / 60, 1),
                           'dump': str(dumps[0]) if dumps else '(WER 未生成)',
                           'req': stat['req'], 'reloads': stat['reloads']}
        print('CRASHED:', json.dumps(stat['crashed'], ensure_ascii=False), flush=True)
        STATE.write_text(json.dumps({**stat, 'req': stat['req'], 'err': stat['err']}, ensure_ascii=False))
    else:
        proc.terminate()
        proc.wait(timeout=20)
        print('SURVIVED %.1fh, req=%d, reloads=%d' % ((time.time() - t0) / 3600, stat['req'], stat['reloads']))


if __name__ == '__main__':
    main()
