# -*- coding: utf-8 -*-
"""reload 风暴钓测：高频主脚本重载 × 大流量乱序全接口乱打。

目标：加速复现"高压流量下的代际切换"缺陷。
- A/B 跑在 gdb 下（退出路径断点武装：ExitProcess/_exit/abort/
  TerminateProcess/NtTerminateProcess/RaiseFailFast + SIGSEGV/SIGABRT），
  C 为无 gdb 对照
- reload 频率：A=45s / B=20s / C=90s（B 最激进）
- 流量：每实例 16 线程，30+ 端点加权乱序池（读列表/视图/会员 API/
  通知 CRUD/附件上传下载/计划任务面板/trace/插件面），10% 混沌请求
  （坏 ID、超长参数、错方法），线程内 0-50ms 随机间隔
- 附加搅动：每 5 分钟对 B 做 8 轮 hello-sdk enable/disable（TCC 代际搅动）
- 任一实例退出：收割 gdb.log/plain.log + note（含精确死亡时刻），
  不再拉起；其余继续

用法: python tests/reload_storm.py [小时数]
"""
import json
import os
import random
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
HOSTEXE = str(ROOT / 'tests' / '.runtime' / 'hostenv' / 'xsw.exe')
HOURS = float(sys.argv[1]) if len(sys.argv) > 1 else 6.0
HARVEST = ROOT / 'tests' / '.runtime' / 'gdb_harvest'
STATE = ROOT / 'tests' / '.runtime' / 'gdb_soak_state.json'

# 端点池：(方法, 路径, 需要谁的身份(None/admin/member), 负载或None, 权重)
def endpoint_pool():
    return [
        ('GET', '/admin/menu', 'admin', None, 6),
        ('GET', '/admin/view/home', 'admin', None, 3),
        ('GET', '/admin/auth/user?page=1&limit=10', 'admin', None, 8),
        ('GET', '/admin/auth/role?page=1&limit=10', 'admin', None, 5),
        ('GET', '/admin/auth/uris?page=1&limit=10', 'admin', None, 5),
        ('GET', '/admin/member/user?page=1&limit=10', 'admin', None, 8),
        ('GET', '/admin/member/group?page=1&limit=10', 'admin', None, 4),
        ('GET', '/admin/logs?page=1&limit=10', 'admin', None, 6),
        ('GET', '/admin/option/get?file=global.json', 'admin', None, 4),
        ('GET', '/admin/option/files', 'admin', None, 3),
        ('GET', '/admin/form/list', 'admin', None, 3),
        ('GET', '/admin/trace/overview', 'admin', None, 2),
        ('GET', '/admin/trace/route', 'admin', None, 2),
        ('GET', '/admin/plugin/list', 'admin', None, 4),
        ('GET', '/admin/sched/tasks?page=1&limit=10', 'admin', None, 6),
        ('GET', '/admin/sched/dashboard', 'admin', None, 5),
        ('GET', '/admin/sched/logs?page=1&limit=10', 'admin', None, 5),
        ('GET', '/admin/attachment/list?page=1&limit=10', 'admin', None, 5),
        ('GET', '/admin/attachment/stats', 'admin', None, 3),
        ('GET', '/admin/member/notify?page=1&limit=10', 'admin', None, 4),
        ('POST', '/admin/member/notify', 'admin', {'sendType': 'users', 'memberIds': '1', 'title': 'storm', 'content': 'x'}, 4),
        ('GET', '/api/v1/profile', 'member', None, 5),
        ('GET', '/api/v1/balance', 'member', None, 4),
        ('GET', '/api/v1/balance/log', 'member', None, 3),
        ('GET', '/api/v1/notify/list', 'member', None, 5),
        ('GET', '/api/v1/notify/unread_count', 'member', None, 5),
        ('POST', '/api/v1/notify/read_all', 'member', {}, 3),
        ('GET', '/api/v1/attachment/my', 'member', None, 4),
        ('GET', '/api/v1/attachment/purchased', 'member', None, 3),
        ('GET', '/', None, None, 4),
        ('GET', '/admin/login', None, None, 4),
        ('POST', '/api/v1/login', None, {'username': 'nobody', 'password': 'wrong'}, 2),
        # 混沌（低权重，混入池由随机直接触发）
        ('GET', '/api/v1/notify/detail?id=-1', 'member', None, 1),
        ('GET', '/admin/auth/user?page=9999999&limit=10', 'admin', None, 1),
        ('GET', '/api/v1/notify/detail?id=1%20OR%201=1', 'member', None, 1),
    ]

CHAOS = [
    ('POST', '/api/v1/notify/read', 'member', {'ids': [random.randint(-5, 10**9) for _ in range(50)]}),
    ('GET', '/admin/logs?page=1&limit=10&search=' + 'X' * 300, 'admin', None),
    ('GET', '/api/v1/notify/detail?id=' + '9' * 40, 'member', None),
    ('POST', '/admin/sched/preview', 'admin', {'scheduleType': 'cron', 'cronExpr': '*/5 * * * * *', 'execType': 'shell', 'codeText': 'x', 'name': 'fz'}),
    ('GET', '/admin/menu?page=1&limit=10&search=' + 'Y' * 200, 'admin', None),
    ('PUT', '/api/v1/profile', 'member', {'nickname': 'Z' * 500}),
]


class Instance:
    def __init__(self, name, port, reload_sec, hammer_n, use_gdb):
        self.name = name
        self.port = port
        self.reload_sec = reload_sec
        self.hammer_n = hammer_n
        self.use_gdb = use_gdb
        self.target = smoke.fixture(port)
        self.proc = None
        self.ck = None
        self.mck = None
        self.xid = None
        self.stop = threading.Event()
        self.stat = {'req': 0, 'err': 0, 'reloads': 0, 'reload_ok': 0, 'crashed': None, 'alive': True}

    def boot(self):
        args = [HOSTEXE, str(self.target / 'xs.json')]
        if self.use_gdb:
            self.proc = subprocess.Popen(
                [GDB, '-q', '-batch', '-x', GDB_CMDS, '--args'] + args,
                cwd=str(self.target), stdout=open(self.target / 'gdb.log', 'wb'),
                stderr=subprocess.STDOUT)
        else:
            self.proc = subprocess.Popen(args, cwd=str(self.target),
                                         stdout=open(self.target / 'plain.log', 'wb'),
                                         stderr=subprocess.STDOUT,
                                         creationflags=subprocess.CREATE_NO_WINDOW)
        c = Client(self.port, timeout=30)
        for _ in range(180):
            if self.proc.poll() is not None:
                raise RuntimeError('%s exited at boot: %s' % (self.name, self._bootlog()))
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
        self.mck = member_cookie(c, 'storm_%s_m' % self.name)
        up = multipart_body({'modelName': self.name}, 'file', 's.zip', b'S' * 512)
        s, _, b_, _ = c.request('POST', '/admin/attachment/upload', up[1], self.ck, up[0])
        r = jload(b_)
        self.xid = r['data']['xid'] if r and r.get('result') else 'x'
        self.pool = endpoint_pool()
        self.total_w = sum(e[4] for e in self.pool)

    def _bootlog(self):
        for f in ('gdb.log', 'plain.log'):
            p = self.target / f
            if p.exists():
                return open(p, errors='replace').read()[-400:]
        return ''

    def start_hammers(self):
        rnd = random.Random(hash(self.name) & 0xFFFFFFFF)

        def hammer(i):
            cc = Client(self.port, timeout=20)
            while not self.stop.is_set():
                try:
                    if rnd.random() < 0.10:
                        m, p, who, payload = CHAOS[rnd.randrange(len(CHAOS))]
                    else:
                        pick = rnd.uniform(0, self.total_w)
                        acc = 0.0
                        for m, p, who, payload, w in self.pool:
                            acc += w
                            if pick <= acc:
                                break
                    cookie = self.ck if who == 'admin' else (self.mck if who == 'member' else None)
                    if p.startswith('/attachment?xid='):
                        p = '/attachment?xid=' + self.xid
                    s, _, _, _ = cc.request(m, p, payload, cookie)
                    self.stat['req'] += 1
                    if s >= 500 or s == 0:
                        self.stat['err'] += 1
                except OSError:
                    self.stat['err'] += 1
                    time.sleep(0.05)
                time.sleep(rnd.uniform(0, 0.05))

        self.threads = [threading.Thread(target=hammer, args=(i,), daemon=True)
                        for i in range(self.hammer_n)]
        for t in self.threads:
            t.start()

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def do_reload(self):
        try:
            s, _, b_, _ = Client(self.port, timeout=15).request('POST', '/__test/reload', None, self.ck, retry=False)
            self.stat['reloads'] += 1
            if s == 202:
                self.stat['reload_ok'] += 1
            return True
        except OSError:
            return False

    def harvest(self, note):
        tag = time.strftime('%H%M%S') + '_%03d' % (int(time.time() * 1000) % 1000)
        out = HARVEST / ('%s_%s' % (self.name, tag))
        out.mkdir(parents=True, exist_ok=True)
        (out / 'note.txt').write_text(note, encoding='utf-8')
        for f in ('gdb.log', 'plain.log'):
            src = self.target / f
            if src.exists():
                (out / f).write_bytes(src.read_bytes()[:600000])
        for f in self.target.glob('core.*'):
            try:
                os.replace(f, out / f.name)
            except OSError:
                pass
        self.stat['crashed'] = str(out)
        print('[harvest] %s -> %s | %s' % (self.name, out, note), flush=True)


def main():
    HARVEST.mkdir(parents=True, exist_ok=True)
    insts = [
        Instance('A', 19910, reload_sec=45, hammer_n=16, use_gdb=True),
        Instance('B', 19911, reload_sec=20, hammer_n=16, use_gdb=True),
        Instance('C', 19912, reload_sec=90, hammer_n=12, use_gdb=False),
    ]
    for inst in insts:
        inst.boot()
        inst.start_hammers()
    print('[storm] 3 instances up: A(gdb,45s) B(gdb,20s) C(plain,90s)', flush=True)

    t0 = time.time()
    last_reload = {i.name: 0.0 for i in insts}
    last_churn = 0.0
    while time.time() - t0 < HOURS * 3600:
        time.sleep(5)
        for inst in insts:
            if not inst.alive():
                if not inst.stat['crashed']:
                    rc = inst.proc.poll()
                    inst.harvest('exit=%s after %.1fmin req=%d reloads=%d(ok=%d) death=%s' % (
                        rc, (time.time() - t0) / 60, inst.stat['req'],
                        inst.stat['reloads'], inst.stat['reload_ok'],
                        time.strftime('%H:%M:%S') + '.%03d' % (int(time.time() * 1000) % 1000)))
                    inst.stop.set()
            elif time.time() - last_reload[inst.name] >= inst.reload_sec:
                last_reload[inst.name] = time.time()
                inst.do_reload()
        # B 的插件启停搅动（每 5 分钟 8 轮）
        if time.time() - last_churn > 300:
            last_churn = time.time()
            b = insts[1]
            if b.alive():
                try:
                    cch = Client(b.port, timeout=15)
                    for _ in range(8):
                        cch.request('POST', '/admin/plugin/disable', {'name': 'hello-sdk'}, b.ck)
                        cch.request('POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, b.ck)
                except OSError:
                    pass
        STATE.write_text(json.dumps({
            'minutes': round((time.time() - t0) / 60, 1),
            **{i.name: i.stat for i in insts}}, ensure_ascii=False), encoding='utf-8')

    for inst in insts:
        inst.stop.set()
        if inst.alive():
            inst.proc.kill()
    print('[storm] finished', flush=True)


if __name__ == '__main__':
    main()
