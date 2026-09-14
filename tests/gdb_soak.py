# -*- coding: utf-8 -*-
"""双实例 GDB 附加长测（钓崩溃现场）。

实例 A：常规节奏（10 分钟重载，复刻历史崩溃工况 6.2h/36 次重载）
实例 B：密集节奏（2 分钟重载，提高单位时间重载次数）
两者均跑在 gdb -batch 下：SIGSEGV/SIGABRT 即停——寄存器、40 帧回溯、
$pc 反汇编、线程清单、core 文件一次抓全。看门狗监测双实例，任一退出即
收割现场到 gdb_harvest/，另一实例继续。

用法: python tests/gdb_soak.py [小时数]
产出: tests/.runtime/gdb_soak_state.json / gdb_harvest/
"""
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke
from campaign_10h import Client, admin_cookie, member_cookie, multipart_body, jload

ROOT = Path(__file__).resolve().parents[1]
GDB = 'E:/software/w64devkit/bin/gdb.exe'
GDB_CMDS = str(ROOT / 'tests' / '.runtime' / 'gdb_cmds3.txt')
HOURS = float(sys.argv[1]) if len(sys.argv) > 1 else 14.0
HARVEST = ROOT / 'tests' / '.runtime' / 'gdb_harvest'
STATE = ROOT / 'tests' / '.runtime' / 'gdb_soak_state.json'


class Instance:
    def __init__(self, name, port, reload_sec, hammer_n):
        self.name = name
        self.port = port
        self.reload_sec = reload_sec
        self.harmer_n = hammer_n
        self.target = smoke.fixture(port)
        self.gdb = None
        self.ck = None
        self.mck = None
        self.xid = None
        self.stop = threading.Event()
        self.stat = {'req': 0, 'err': 0, 'reloads': 0, 'crashed': None, 'alive': True}

    def boot(self):
        self.gdb = subprocess.Popen(
            [GDB, '-q', '-batch', '-x', GDB_CMDS, '--args',
             str(ROOT / 'tests' / '.runtime' / 'hostenv' / 'xsw.exe'), str(self.target / 'xs.json')],
            cwd=str(self.target),
            stdout=open(self.target / 'gdb.log', 'wb'),
            stderr=subprocess.STDOUT)
        c = Client(self.port, timeout=30)
        for _ in range(180):
            if self.gdb.poll() is not None:
                raise RuntimeError('%s gdb exited at boot: %s' % (
                    self.name, open(self.target / 'gdb.log', errors='replace').read()[-500:]))
            try:
                if c.request('GET', '/admin/login')[0] == 200:
                    break
            except OSError:
                pass
            time.sleep(0.5)
        else:
            raise RuntimeError('%s not ready' % self.name)
        import sqlite3
        with sqlite3.connect(self.target / 'db/main.db') as db:
            db.execute('UPDATE sched_task SET enabled = 0')
            db.commit()
        self.ck = admin_cookie(c)
        self.mck = member_cookie(c, 'gdb_%s_m' % self.name)
        up = multipart_body({'modelName': self.name}, 'file', 'g.zip', b'G' * 1024)
        s, _, b_, _ = c.request('POST', '/admin/attachment/upload', up[1], self.ck, up[0])
        self.xid = jload(b_)['data']['xid']
        self.stat['alive'] = True

    def start_hammers(self):
        def hammer(i):
            cc = Client(self.port, timeout=20)
            paths = ['/admin/auth/user?page=1&limit=10', '/api/v1/notify/unread_count',
                     '/attachment?xid=' + (self.xid or 'x'), '/admin/sched/dashboard']
            while not self.stop.is_set():
                try:
                    s, _, _, _ = cc.request('GET', paths[i % 4], None,
                                            self.ck if i % 4 in (0, 3) else self.mck)
                    self.stat['req'] += 1
                    if s >= 500 or s == 0:
                        self.stat['err'] += 1
                except OSError:
                    self.stat['err'] += 1
                    time.sleep(0.05)
        self.threads = [threading.Thread(target=hammer, args=(i,), daemon=True)
                        for i in range(self.harmer_n)]
        for t in self.threads:
            t.start()

    def alive(self):
        return self.gdb is not None and self.gdb.poll() is None

    def harvest(self, note):
        tag = time.strftime('%H%M%S')
        out = HARVEST / ('%s_%s' % (self.name, tag))
        out.mkdir(parents=True, exist_ok=True)
        for f in ('gdb.log',):
            src = self.target / f
            if src.exists():
                (out / f).write_bytes(src.read_bytes()[:400000])
        for f in self.target.glob('core.*'):
            try:
                os.replace(f, out / f.name)
            except OSError:
                pass
        (out / 'note.txt').write_text(note, encoding='utf-8')
        self.stat['crashed'] = str(out)
        print('[harvest] %s -> %s' % (self.name, out), flush=True)


class PlainInstance(Instance):
    """无 gdb 对照：裸 xs.exe + 死亡时刻/退出码记录。"""
    def boot(self):
        import subprocess as sp
        self.gdb = sp.Popen([str(ROOT / 'tests' / '.runtime' / 'hostenv' / 'xsw.exe'), str(self.target / 'xs.json')],
                            cwd=str(self.target),
                            stdout=open(self.target / 'plain.log', 'wb'),
                            stderr=sp.STDOUT,
                            creationflags=sp.CREATE_NO_WINDOW)
        c = Client(self.port, timeout=30)
        for _ in range(180):
            if self.gdb.poll() is not None:
                raise RuntimeError('%s exited at boot' % self.name)
            try:
                if c.request('GET', '/admin/login')[0] == 200:
                    break
            except OSError:
                pass
            time.sleep(0.5)
        import sqlite3
        with sqlite3.connect(self.target / 'db/main.db') as db:
            db.execute('UPDATE sched_task SET enabled = 0')
            db.commit()
        self.ck = admin_cookie(c)
        self.mck = member_cookie(c, 'plain_%s_m' % self.name)
        self.xid = 'x'
        self.stat['alive'] = True

    def harvest(self, note):
        tag = time.strftime('%H%M%S')
        out = HARVEST / ('%s_%s' % (self.name, tag))
        out.mkdir(parents=True, exist_ok=True)
        (out / 'note.txt').write_text(note + chr(10) + 'death_mtime=' + time.strftime('%H:%M:%S') + '.%03d' % (int(time.time() * 1000) % 1000), encoding='utf-8')
        src = self.target / 'plain.log'
        if src.exists():
            (out / 'plain.log').write_bytes(src.read_bytes()[:400000])
        self.stat['crashed'] = str(out)
        print('[harvest] %s -> %s' % (self.name, out), flush=True)


def main():
    HARVEST.mkdir(parents=True, exist_ok=True)
    inst_a = Instance('A', 19910, reload_sec=600, hammer_n=12)
    inst_b = Instance('B', 19911, reload_sec=120, hammer_n=8)
    inst_c = PlainInstance('C', 19912, reload_sec=300, hammer_n=6)
    inst_a.boot()
    inst_a.start_hammers()
    inst_b.boot()
    inst_b.start_hammers()
    inst_c.boot()
    inst_c.start_hammers()
    print('[gdb_soak] three instances up (A/B under gdb, C plain)', flush=True)

    t0 = time.time()
    last_reload = {'A': 0.0, 'B': 0.0, 'C': 0.0}
    while time.time() - t0 < HOURS * 3600:
        time.sleep(10)
        for inst in (inst_a, inst_b, inst_c):
            if not inst.alive():
                if not inst.stat['crashed']:
                    inst.stat['alive'] = False
                    rc = inst.gdb.poll()
                    inst.harvest('exit=%s after %.1f min, req=%d, reloads=%d' % (
                        rc, (time.time() - t0) / 60, inst.stat['req'], inst.stat['reloads']))
                    inst.stop.set()
            elif time.time() - last_reload[inst.name] > inst.reload_sec:
                last_reload[inst.name] = time.time()
                try:
                    Client(inst.port, timeout=15).request('POST', '/__test/reload', None, inst.ck)
                    inst.stat['reloads'] += 1
                except OSError:
                    pass
        STATE.write_text(json.dumps({
            'minutes': round((time.time() - t0) / 60, 1),
            'A': inst_a.stat, 'B': inst_b.stat, 'C': inst_c.stat}, ensure_ascii=False), encoding='utf-8')

    for inst in (inst_a, inst_b):
        inst.stop.set()
        if inst.alive():
            inst.gdb.kill()
    print('[gdb_soak] finished', flush=True)


if __name__ == '__main__':
    main()
