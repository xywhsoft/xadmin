# -*- coding: utf-8 -*-
"""7 小时混合极端压力战役：GDB 启动 + 全接口持续请求 + 压测/渗透/重载风暴混合。

目标（用户指令）：找 HTTP API 漏洞、程序 BUG、重载崩溃、压力性能薄弱点。
编排：
  - 服务器经 GDB batch 启动（SIGSEGV/SIGABRT/SIGFPE/SIGILL 捕获 → bt 40 + 寄存器）；
    崩溃后自动归档日志与请求上下文（最近 256 笔）并重启续测。
  - 七类混合负载（daemon 线程池）：
      reader        热点只读 GET（页面/公开站点/管理 API）
      writer        CRUD 冲刷（用户/角色/菜单/会员/计划任务，自清理 + DB 抽验）
      fuzzer        渗透载荷（SQLi/XSS/穿越/类型混淆/深嵌套/超长/空字节）
      reload        重载风暴（/__test/reload 5-15s 随机，会话自愈）
      session churn 管理员与会员 登录/登出/资料 循环
      plugin beater 插件启停 + guestbook 公开写 + xlog push
      uploader      multipart 上传（1KB/100KB/2MB + 文件名穿越探测）
      cycler        全端点清单顺序循环（覆盖长尾接口）
  - 指标：每 30s 聚合（p50/p95/p99、eps、错误分类、重载邻域错误）；内存/句柄采样。
  - 5xx/连接重置/空响应/超长响应 全部带载荷上下文落 findings。

产出：tests/.runtime/extreme7/{campaign.log, metrics.jsonl, findings.json,
state.json, gdb_run_NNN.log, crash_NNN_context.json}
只攻击一次性夹具；根库与仓库只读（本文件自身除外）。
"""
from collections import deque
from pathlib import Path
import ctypes
import hashlib
import http.client
import json
import os
import random
import socket
import sqlite3
import statistics
import subprocess
import sys
import threading
import time
import traceback
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'tests' / '.runtime' / 'extreme7'
GDB = Path(r'E:\software\w64devkit\bin\gdb.exe')
ADMIN = 'migration_smoke'
PASSWORD = 'Temporary-test-only-9081'
PORT = 18600
DURATION_H = float(sys.argv[1]) if len(sys.argv) > 1 else 7.0
USE_GDB = '--no-gdb' not in sys.argv
PLUGINS = ('xlogserver', 'guestbook_v3', 'filemanager', 'hello-sdk')

# ==================== 共享状态 ====================
LOCK = threading.Lock()
FINDINGS = []
METRICS = []
RING = deque(maxlen=256)          # (t, method, path, bucket)
LAST_RELOAD = [0.0]
STATS = {'req': 0, 'ok': 0, 'err4xx': 0, 'err5xx': 0, 'connerr': 0,
         'reload_prox_err': 0, 'reloads': 0, 'crashes': 0, 'boots': 0}
LAT = {}                            # bucket -> [latency, ...]（采样窗口内）
STATE = {'phase': 'boot', 'started': time.time()}


def now_str():
    return time.strftime('%H:%M:%S')


def log(msg):
    print('[%s] %s' % (now_str(), msg), flush=True)


def finding(fid, severity, title, detail):
    with LOCK:
        FINDINGS.append({'id': fid, 'severity': severity, 'title': title,
                         'detail': detail, 'at': time.strftime('%m-%d %H:%M:%S')})
        (RUNTIME / 'findings.json').write_text(
            json.dumps(FINDINGS, ensure_ascii=False, indent=1), encoding='utf-8')
    log('FINDING %s %s: %s | %s' % (severity, fid, title, detail))


def save_state():
    STATE['elapsed_h'] = round((time.time() - STATE['started']) / 3600, 2)
    STATE['stats'] = dict(STATS)
    (RUNTIME / 'state.json').write_text(
        json.dumps(STATE, ensure_ascii=False, indent=1), encoding='utf-8')


def ring_push(method, path, bucket):
    with LOCK:
        RING.append((time.time(), method, path, bucket))


def record(bucket, status, latency, method, path):
    with LOCK:
        STATS['req'] += 1
        LAT.setdefault(bucket, []).append(latency)
        if status is None:
            STATS['connerr'] += 1
        elif status < 400:
            STATS['ok'] += 1
        elif status < 500:
            STATS['err4xx'] += 1
        else:
            STATS['err5xx'] += 1
        if (status is None or status >= 500) and time.time() - LAST_RELOAD[0] < 10:
            STATS['reload_prox_err'] += 1
    ring_push(method, path, bucket)


# ==================== HTTP 客户端（线程私有连接 + 自动重连） ====================
class Client:
    def __init__(self, timeout=15):
        self.timeout = timeout
        self.conn = None

    def _connect(self):
        self.conn = http.client.HTTPConnection('127.0.0.1', PORT, timeout=self.timeout)

    def request(self, method, path, body=None, cookie=None, ctype=None, bucket='misc', timeout=None):
        t0 = time.perf_counter()
        headers = {}
        if body is not None:
            headers['Content-Type'] = ctype or 'application/json'
        if cookie:
            headers['Cookie'] = cookie
        for attempt in (0, 1):
            try:
                if self.conn is None:
                    self._connect()
                self.conn.request(method, path, body, headers)
                r = self.conn.getresponse()
                data = r.read()
                lat = time.perf_counter() - t0
                record(bucket, r.status, lat, method, path)
                return r.status, dict(r.getheaders()), data
            except Exception:
                try:
                    self.conn.close()
                except Exception:
                    pass
                self.conn = None
                if attempt == 1:
                    record(bucket, None, time.perf_counter() - t0, method, path)
                    return None, {}, b''
                time.sleep(0.05 * random.random())

    def jreq(self, method, path, obj=None, cookie=None, bucket='misc'):
        body = json.dumps(obj).encode() if obj is not None else None
        st, h, b = self.request(method, path, body, cookie, bucket=bucket)
        try:
            return st, json.loads(b)
        except Exception:
            return st, None


def client_hash(user, pwd):
    return hashlib.sha256((user + '_xywhsoft_' + pwd).encode()).hexdigest()


def admin_login(c):
    for _ in range(10):
        st, d = c.jreq('POST', '/admin/login',
                        {'username': ADMIN, 'password': client_hash(ADMIN, PASSWORD)}, bucket='login')
        if st == 200 and d and d.get('result'):
            return True
        time.sleep(0.5)
    return False


def get_cookie(c):
    st, h, b = c.request('POST', '/admin/login',
                          json.dumps({'username': ADMIN,
                                      'password': client_hash(ADMIN, PASSWORD)}).encode(),
                          bucket='login')
    if st == 200:
        sc = h.get('Set-Cookie', '')
        if 'XSID=' in sc:
            return sc.split(';')[0]
    return None


# ==================== 内存采样（ctypes，避免 powershell 开销） ====================
class MemSampler:
    def __init__(self):
        self.pid = None

    def set_pid(self, pid):
        self.pid = pid

    def read(self):
        if not self.pid:
            return -1, -1
        try:
            psapi = ctypes.WinDLL('psapi')
            kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
            class PMC(ctypes.Structure):
                _fields_ = [('cb', ctypes.c_uint32), ('PageFaultCount', ctypes.c_uint32),
                            ('PeakWorkingSetSize', ctypes.c_size_t), ('WorkingSetSize', ctypes.c_size_t),
                            ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                            ('QuotaPagedPoolUsage', ctypes.c_size_t),
                            ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                            ('QuotaNonPagedPoolUsage', ctypes.c_size_t),
                            ('PagefileUsage', ctypes.c_size_t), ('PeakPagefileUsage', ctypes.c_size_t)]
            mc = PMC()
            mc.cb = ctypes.sizeof(PMC)
            h = kernel32.OpenProcess(0x0400 | 0x0010, False, self.pid)  # QUERY_INFORMATION|VM_READ
            if not h:
                return -1, -1
            try:
                if psapi.GetProcessMemoryInfo(h, ctypes.byref(mc), mc.cb):
                    return mc.WorkingSetSize // 1048576, mc.PageFaultCount
                return -1, -1
            finally:
                kernel32.CloseHandle(h)
        except Exception:
            return -1, -1


MEM = MemSampler()


def find_server_pid():
    try:
        out = subprocess.run(['netstat', '-ano'], capture_output=True,
                            timeout=15).stdout.decode('utf-8', 'replace')
        for line in out.splitlines():
            if 'LISTENING' in line and (':%d ' % PORT) in line:
                return int(line.split()[-1])
    except Exception:
        pass
    return None


# ==================== GDB 服务器管理 ====================
class GdbServer:
    def __init__(self):
        self.proc = None
        self.run_idx = 0
        self.target = None

    def fixture(self):
        target = smoke.fixture(PORT)
        # 打开全部被测插件
        with sqlite3.connect(target / 'db/main.db') as db:
            db.execute("UPDATE plugin_runtime SET enabled=0")
            db.execute("UPDATE plugin_runtime SET enabled=1 WHERE xid IN (%s)"
                       % ','.join("'%s'" % x for x in PLUGINS))
            db.commit()
        self.target = target
        return target

    def boot(self):
        self.run_idx += 1
        STATS['boots'] += 1
        logf = open(RUNTIME / ('gdb_run_%03d.log' % self.run_idx), 'wb')
        if USE_GDB:
            cmd = [str(GDB), '-batch',
                   '-ex', 'set pagination off',
                   '-ex', 'set confirm off',
                   '-ex', 'handle SIGSEGV stop print nopass',
                   '-ex', 'handle SIGABRT stop print nopass',
                   '-ex', 'handle SIGFPE stop print nopass',
                   '-ex', 'handle SIGILL stop print nopass',
                   '-ex', 'run',
                   '-ex', 'echo \\n===STOP===\\n',
                   '-ex', 'bt 40',
                   '-ex', 'info registers',
                   '-ex', 'quit',
                   '--args', str(ROOT / 'xs.exe'), str(self.target / 'xs.json')]
        else:
            cmd = [str(ROOT / 'xs.exe'), str(self.target / 'xs.json')]
        self.proc = subprocess.Popen(cmd, cwd=str(ROOT), stdout=logf, stderr=subprocess.STDOUT,
                                     creationflags=0x00000008)  # DETACHED_PROCESS
        # 等端口就绪
        c = Client(timeout=5)
        for _ in range(240):
            if self.proc.poll() is not None:
                raise RuntimeError('boot died, see gdb_run_%03d.log' % self.run_idx)
            try:
                if c.request('GET', '/admin/login', bucket='boot')[0] == 200:
                    MEM.set_pid(find_server_pid())
                    STATE['mem_reported'] = False
                    log('server up (run %d, xs pid %s)' % (self.run_idx, MEM.pid))
                    return True
            except Exception:
                pass
            time.sleep(0.25)
        raise RuntimeError('server not ready')

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def archive_crash(self):
        STATS['crashes'] += 1
        n = STATS['crashes']
        src = RUNTIME / ('gdb_run_%03d.log' % self.run_idx)
        dst = RUNTIME / ('crash_%03d_gdb.log' % n)
        try:
            if src.exists():
                import shutil
                shutil.copyfile(src, dst)
        except Exception:
            pass
        with LOCK:
            ctx = list(RING)
        (RUNTIME / ('crash_%03d_context.json' % n)).write_text(
            json.dumps([{'t': round(t, 3), 'm': m, 'p': p, 'b': b} for t, m, p, b in ctx],
                       ensure_ascii=False, indent=1), encoding='utf-8')
        tail = ''
        try:
            tail = src.read_text(errors='replace')[-1500:]
        except Exception:
            pass
        finding('CRASH-%d' % n, 'critical', '服务器崩溃（GDB 捕获）',
                'run=%d 详情见 crash_%03d_gdb.log；最近请求见 crash_%03d_context.json\n%s'
                % (self.run_idx, n, n, tail[:600]))

    def stop(self):
        pid = find_server_pid()
        if pid:
            subprocess.run(['taskkill', '/PID', str(pid), '/T', '/F'],
                           capture_output=True, timeout=30)
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
                self.proc.wait(timeout=15)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass


SRV = GdbServer()


# ==================== 渗透载荷库 ====================
SQLI = ["' OR '1'='1", "1';DROP TABLE user--", "1' UNION SELECT 1,2,3--", "admin'--",
        "' OR sleep(1)--", "%%", "%_", "\\", "';--", "1 OR 1=1"]
XSS = ['<script>alert(1)</script>', '"><img src=x onerror=alert(1)>',
       'javascript:alert(1)', '<svg/onload=alert(1)>', '{{7*7}}', '{{$user}}']
TRAVERSAL = ['../../main.db', '..\\..\\main.db', '%2e%2e%2f%2e%2e%2fmain.db',
             '....//....//main.db', '/etc/passwd', 'C:\\Windows\\win.ini',
             '..%c0%af..%c0%af', '....\\\\....\\\\']
BADNUM = [0, -1, 99999999999999999999, -9223372036854775808, 2147483648, 1e18, 'abc', None, [], {}]


def deep_json(depth=400):
    root = {}
    cur = root
    for _ in range(depth):
        cur['a'] = {}
        cur = cur['a']
    cur['x'] = 1
    return root


def big_str(n=100000):
    return 'A' * n


def fuzz_value(rng):
    r = rng.random()
    if r < 0.22:
        return rng.choice(SQLI)
    if r < 0.44:
        return rng.choice(XSS)
    if r < 0.66:
        return rng.choice(TRAVERSAL)
    if r < 0.78:
        return rng.choice(BADNUM)
    if r < 0.86:
        return '\u0000' + 'x' * rng.randint(1, 64)
    if r < 0.94:
        return big_str(rng.randint(2000, 60000))
    return deep_json(rng.randint(50, 300))


# ==================== 负载线程 ====================
READS = [
    ('/', 'public'), ('/features', 'public'), ('/docs', 'public'), ('/download', 'public'),
    ('/admin/login', 'page'), ('/admin/view/home', 'page'),
    ('/admin/auth/user?page=1&limit=10', 'api'), ('/admin/auth/role?page=1&limit=10', 'api'),
    ('/admin/auth/group?page=1&limit=10', 'api'), ('/admin/auth/uris?page=1&limit=10', 'api'),
    ('/admin/member/user?page=1&limit=10', 'api'), ('/admin/option/menu', 'api'),
    ('/admin/plugin/list?page=1&limit=20', 'api'), ('/admin/sched/tasks', 'api'),
    ('/admin/attachment/list?page=1&limit=10', 'api'), ('/admin/attachment/stats', 'api'),
    ('/admin/logs?page=1&limit=10', 'api'), ('/admin/trace/route', 'api'),
    ('/api/v1/login', 'pubapi'),
]

INVENTORY = [p for _, p in READS] + [
    '/admin/view/attachment', '/admin/view/attachment/stats', '/admin/view/auth/user',
    '/admin/view/auth/role', '/admin/view/auth/group', '/admin/view/auth/auth',
    '/admin/view/auth/uris', '/admin/view/member/user', '/admin/view/member/group',
    '/admin/view/member/notify', '/admin/view/member/mail', '/admin/view/option/files',
    '/admin/view/option/menu', '/admin/view/plugin', '/admin/view/plugin/store',
    '/admin/view/sched', '/admin/view/sched/dashboard', '/admin/view/sched/log',
    '/admin/view/logs', '/admin/view/tool/reload', '/admin/view/form',
    '/admin/view/content', '/admin/view/content/page', '/admin/view/content/packs',
    '/admin/content/types', '/admin/content/templates?page=1&limit=5', '/admin/content/packs',
    '/admin/content/pages?page=1&limit=5', '/admin/content/generations?page=1&limit=5',
    '/admin/member/mail/status', '/admin/sched/dashboard', '/admin/sched/logs?page=1&limit=5',
    '/admin/trace', '/admin/trace/session', '/admin/trace/option', '/admin/trace/auth',
    '/plugins', '/capabilities', '/content-system', '/demo',
    '/api/plugin/guestbook_v3/meta', '/api/plugin/guestbook_v3/list?page=1&limit=5',
    '/admin/view/plugin/xlogserver',
]


def worker_reader(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=20)
    ck = None
    while not stop.is_set():
        path, bucket = rng.choice(READS)
        use_ck = '/admin' in path
        if use_ck and not ck:
            ck = get_cookie(c)
        c.request('GET', path, None, ck if use_ck else None, bucket=bucket)
        time.sleep(rng.uniform(0.005, 0.05))


def worker_writer(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=25)
    ck = None
    seq = 0
    while not stop.is_set():
        seq += 1
        if not ck:
            admin_login(c)
            ck = get_cookie(c)
            if not ck:
                time.sleep(1)
                continue
        name = 'ext_w_%d_%d' % (seed, seq)
        st, d = c.jreq('POST', '/admin/auth/user',
                        {'username': name, 'password': client_hash(name, PASSWORD),
                         'role': 1, 'authLevel': 0}, cookie=ck, bucket='write')
        if st == 302:
            ck = None
            continue
        try:
            with sqlite3.connect(SRV.target / 'db/main.db') as db:
                row = db.execute("SELECT id FROM user WHERE user=? AND isDelete=0", (name,)).fetchone()
            if row:
                uid = row[0]
                c.jreq('PUT', '/admin/auth/user', {'id': uid, 'role': 1, 'authLevel': 5},
                       cookie=ck, bucket='write')
                c.request('DELETE', '/admin/auth/user?id=%d' % uid, None, ck, bucket='write')
            else:
                finding('WRT-D%d' % seq, 'medium', '建用户后 DB 查无记录',
                        'name=%s resp=%s（假成功候选）' % (name, str(d)[:120]))
        except Exception:
            pass
        # 角色冲刷
        rname = 'ext_r_%d_%d' % (seed, seq)
        st, d = c.jreq('POST', '/admin/auth/role', {'name': rname, 'desc': 'x'},
                        cookie=ck, bucket='write')
        if st == 302:
            ck = None
        else:
            try:
                with sqlite3.connect(SRV.target / 'db/main.db') as db:
                    row = db.execute("SELECT id FROM role WHERE name=? AND isDelete=0", (rname,)).fetchone()
                if row:
                    c.request('DELETE', '/admin/auth/role?id=%d' % row[0], None, ck, bucket='write')
            except Exception:
                pass
        time.sleep(rng.uniform(0.1, 0.6))


def worker_fuzzer(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=25)
    ck = None
    targets_json = [
        ('POST', '/admin/auth/user'), ('PUT', '/admin/auth/user'),
        ('POST', '/admin/auth/role'), ('PUT', '/admin/auth/uris'),
        ('POST', '/admin/member/user'), ('PUT', '/admin/member/user'),
        ('POST', '/admin/option/menu'), ('PUT', '/admin/option/menu'),
        ('POST', '/admin/sched/task/save'), ('POST', '/api/v1/register'),
        ('POST', '/api/v1/login'), ('POST', '/api/v1/profile'),
        ('POST', '/admin/login'), ('POST', '/api/plugin/guestbook_v3/add'),
        ('POST', '/api/v1/log/push'),
    ]
    targets_query = [
        ('GET', '/admin/auth/user?page=1&limit=10&search='),
        ('GET', '/admin/member/user?page=1&limit=10&search='),
        ('GET', '/admin/logs?page=1&limit=10&uri='),
        ('GET', '/admin/option/file?file='),
        ('GET', '/admin/view/option?file='),
        ('GET', '/attachment?xid='),
        ('GET', '/admin/view/option/menu/edit?id='),
        ('GET', '/admin/auth/user/edit?id='),
    ]
    while not stop.is_set():
        if rng.random() < 0.6:
            m, path = rng.choice(targets_json)
            if not ck and '/admin' in path:
                ck = get_cookie(c)
            body = {'name': fuzz_value(rng), 'title': fuzz_value(rng),
                    'username': fuzz_value(rng), 'search': fuzz_value(rng),
                    'id': rng.choice(BADNUM), 'desc': fuzz_value(rng),
                    'uris': fuzz_value(rng), 'content': fuzz_value(rng),
                    'cronExpr': fuzz_value(rng), 'text': fuzz_value(rng)}
            if m == 'POST' and path == '/api/v1/log/push':
                body = {'service': rng.choice(BADNUM), 'task': rng.choice(BADNUM),
                        'class': fuzz_value(rng), 'text': fuzz_value(rng)}
            if path == '/api/v1/profile':
                body = {'nickname': fuzz_value(rng), 'email': fuzz_value(rng),
                        'phone': fuzz_value(rng)}
            st, h, b = c.request(m, path, json.dumps(body, default=str).encode('utf-8', 'replace'),
                                  ck if '/admin' in path else None, bucket='fuzz')
            if st is not None and st >= 500:
                finding('FUZ-5xx-%d' % (int(time.time()) % 100000), 'high',
                        '渗透载荷触发 5xx', '%s %s body=%s → %s %s'
                        % (m, path, str(body)[:150], st, b[:150]))
        else:
            m, path = rng.choice(targets_query)
            payload = rng.choice(SQLI + XSS + TRAVERSAL) if 'file=' not in path and 'xid=' not in path \
                else rng.choice(TRAVERSAL)
            from urllib.parse import quote
            st, h, b = c.request(m, path + quote(payload), None,
                                  ck if '/admin' in path else None, bucket='fuzz')
            if st is not None and st >= 500:
                finding('FUZQ-5xx-%d' % (int(time.time()) % 100000), 'high',
                        '查询渗透触发 5xx', '%s %s%s → %s' % (m, path, payload, st))
        if st == 302 and '/admin' in path:
            ck = None
        time.sleep(rng.uniform(0.05, 0.3))


def worker_reload(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=30)
    ck = None
    while not stop.is_set():
        if not ck:
            ck = get_cookie(c)
        if ck:
            st, h, b = c.request('POST', '/__test/reload', b'', ck, bucket='reload')
            if st == 202:
                with LOCK:
                    STATS['reloads'] += 1
                    LAST_RELOAD[0] = time.time()
                time.sleep(6)  # 等代际切换完成
            else:
                ck = None
        time.sleep(rng.uniform(30, 75))  # 泄漏已证实(28MB/代)：降频拉长单代窗口，熔断兜底


def worker_session(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=20)
    seq = 0
    while not stop.is_set():
        seq += 1
        # 管理员
        st, d = c.jreq('POST', '/admin/login',
                        {'username': ADMIN, 'password': client_hash(ADMIN, PASSWORD)}, bucket='login')
        ck = None
        if st == 200:
            # 错误密码（暴破形态，少量）
            c.jreq('POST', '/admin/login', {'username': ADMIN, 'password': 'f' * 64}, bucket='login')
        # 会员注册/登录/资料/登出
        mname = 'ext_m_%d_%d' % (seed, seq)
        c.jreq('POST', '/api/v1/register',
                {'username': mname, 'password': client_hash(mname, PASSWORD)}, bucket='reg')
        st2, d2 = c.jreq('POST', '/api/v1/login',
                          {'username': mname, 'password': client_hash(mname, PASSWORD)}, bucket='login')
        if st2 == 200 and d2 and d2.get('code') == 0:
            c.jreq('POST', '/api/v1/profile', {'nickname': 'churn'}, bucket='profile')
        time.sleep(rng.uniform(0.2, 1.0))


def worker_plugin(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=25)
    ck = None
    while not stop.is_set():
        if not ck:
            admin_login(c)
            ck = get_cookie(c)
        if ck:
            xid = rng.choice(PLUGINS)
            c.request('POST', '/admin/plugin/disable', json.dumps({'name': xid}).encode(), ck, bucket='plugin')
            time.sleep(rng.uniform(0.3, 1.0))
            c.request('POST', '/admin/plugin/enable', json.dumps({'name': xid}).encode(), ck, bucket='plugin')
        # 公开写面
        c.jreq('POST', '/api/plugin/guestbook_v3/add',
               {'nickname': 'beat', 'content': 'beater-%d' % int(time.time())}, bucket='gbwrite')
        c.jreq('POST', '/api/v1/log/push',
               {'service': 1, 'task': 1, 'class': 'info', 'text': 'beat %d' % int(time.time())},
               bucket='xlogpush')
        time.sleep(rng.uniform(0.5, 2.5))


def worker_uploader(stop, seed):
    rng = random.Random(seed)
    c = Client(timeout=60)
    ck = None
    fnames = ['ok_%d.bin', '../../esc_%d.bin', '..\\..\\esc_%d.bin', '/abs/esc_%d.bin',
              '.htaccess_%d', 'x' * 200 + '_%d.bin']
    while not stop.is_set():
        if not ck:
            admin_login(c)
            ck = get_cookie(c)
        if not ck:
            time.sleep(1)
            continue
        size = rng.choice([1024, 65536, 2097152])
        fname = rng.choice(fnames) % int(time.time())
        content = os.urandom(min(size, 65536)) * (size // 65536 if size > 65536 else 1)
        b = uuid.uuid4().hex[:16].encode()
        body = (b'--' + b + b'\r\nContent-Disposition: form-data; name="file"; filename="' +
                fname.encode() + b'"\r\nContent-Type: application/octet-stream\r\n\r\n' +
                content + b'\r\n--' + b + b'--\r\n')
        st, h, resp = c.request('POST', '/admin/attachment/upload', body, ck,
                                 ctype='multipart/form-data; boundary=' + b.decode(), bucket='upload')
        if st == 302:
            ck = None
        time.sleep(rng.uniform(1.0, 4.0))


def worker_cycler(stop, seed):
    c = Client(timeout=20)
    ck = None
    idx = 0
    while not stop.is_set():
        path = INVENTORY[idx % len(INVENTORY)]
        idx += 1
        if '/admin' in path and not ck:
            ck = get_cookie(c)
        c.request('GET', path, None, ck if '/admin' in path else None, bucket='cycle')
        time.sleep(0.08)


# ==================== 指标与看门狗 ====================
def metrics_loop(stop):
    n = 0
    while not stop.is_set():
        time.sleep(30)
        n += 1
        with LOCK:
            window = {k: v for k, v in LAT.items()}
            LAT.clear()
            snap = dict(STATS)
        rss, faults = MEM.read()
        row = {'t': round(time.time(), 1), 'n': n, 'rss_mb': rss, 'pagefaults': faults}
        for bucket, lats in window.items():
            if not lats:
                continue
            s = sorted(lats)
            row[bucket] = {
                'n': len(s),
                'p50': round(statistics.median(s) * 1000, 1),
                'p95': round(s[int(len(s) * 0.95) - 1] * 1000, 1) if len(s) > 1 else None,
                'p99': round(s[min(len(s) - 1, int(len(s) * 0.99)) - 0] * 1000, 1) if len(s) > 5 else None,
                'max': round(s[-1] * 1000, 1),
            }
        row['stats'] = snap
        with open(RUNTIME / 'metrics.jsonl', 'a', encoding='utf-8') as f:
            f.write(json.dumps(row, ensure_ascii=False) + '\n')
        # 性能薄弱点：p95 超阈值（内存高压期的慢是系统性的，不算端点缺陷）
        if rss < 1200:
            for bucket, agg in row.items():
                if isinstance(agg, dict) and agg.get('p95') and agg['p95'] > 5000 and agg.get('n', 0) > 10:
                    finding('PERF-%s-%d' % (bucket, n), 'medium', '性能薄弱点：p95>5s',
                            '%s p95=%sms p99=%sms n=%s' % (bucket, agg['p95'], agg.get('p99'), agg['n']))
        # 内存增长告警（>1.5GB，每代只报一次防刷屏）
        if rss > 1536 and not STATE.get('mem_reported'):
            STATE['mem_reported'] = True
            finding('MEM-%d' % n, 'high', '内存超 1.5GB（重载泄漏）', 'rss=%dMB' % rss)
        log('t+%dmin | req=%s ok=%s 5xx=%s conn=%s reload=%s crash=%s rss=%sMB'
            % (int((time.time() - STATE['started']) / 60), snap['req'], snap['ok'],
               snap['err5xx'], snap['connerr'], snap['reloads'], snap['crashes'], rss))
        save_state()


def db_sanity_loop(stop):
    n = 0
    while not stop.is_set():
        time.sleep(120)
        n += 1
        try:
            with sqlite3.connect(SRV.target / 'db/main.db') as db:
                users = db.execute("SELECT COUNT(*) FROM user WHERE user LIKE 'ext_w_%' AND isDelete=0").fetchone()[0]
                logs_n = db.execute("SELECT COUNT(*) FROM logs").fetchone()[0]
            if users > 500:
                finding('DB-%d' % n, 'medium', 'writer 残留用户堆积', 'ext_w 行=%d（清理路径漏）' % users)
            log('db sanity: ext_users=%d logs=%d' % (users, logs_n))
        except Exception as e:
            log('db sanity err: %s' % e)


# ==================== 主控 ====================
def main():
    RUNTIME.mkdir(parents=True, exist_ok=True)
    STATE['phase'] = 'fixture'
    log('extreme7 启动：%.1fh gdb=%s port=%d' % (DURATION_H, USE_GDB, PORT))
    SRV.fixture()
    STATE['phase'] = 'boot'
    SRV.boot()

    deadline = time.time() + DURATION_H * 3600
    stop = threading.Event()
    threads = []
    specs = [
        (worker_reader, 6), (worker_writer, 3), (worker_fuzzer, 4),
        (worker_reload, 1), (worker_session, 2), (worker_plugin, 2),
        (worker_uploader, 1), (worker_cycler, 1),
    ]
    seed = 1000
    for fn, cnt in specs:
        for i in range(cnt):
            t = threading.Thread(target=fn, args=(stop, seed + i), daemon=True)
            t.start()
            threads.append(t)
            seed += 100
    threading.Thread(target=metrics_loop, args=(stop,), daemon=True).start()
    threading.Thread(target=db_sanity_loop, args=(stop,), daemon=True).start()

    STATE['phase'] = 'running'
    save_state()
    while time.time() < deadline:
        time.sleep(5)
        # 内存熔断：重载泄漏已证实（leg1: 28.1MB/代线性），2.6GB 保护整机
        # ——干净重启续测（非崩溃），每周期落一条 critical 记录
        if time.time() - STATE['started'] > 120:
            rss_now = MEM.read()[0]
            if rss_now > 2600:
                STATS['leak_cycles'] = STATS.get('leak_cycles', 0) + 1
                n = STATS['leak_cycles']
                with LOCK:
                    rl = STATS['reloads']
                finding('LEAK-%d' % n, 'critical', '重换代内存泄漏（熔断重启）',
                        'rss=%dMB 触发 2.6GB 熔断；本代 reloads=%d（≈%.1fMB/代）'
                        % (rss_now, rl, rss_now / max(1, rl)))
                SRV.stop()
                time.sleep(2)
                try:
                    SRV.boot()
                    log('leak cycle %d: restarted' % n)
                except Exception as e:
                    finding('BOOT-FAIL', 'critical', '熔断后重启失败', str(e))
                    break
                continue
        if not SRV.alive():
            log('!! server process exited — archiving crash')
            SRV.archive_crash()
            try:
                SRV.boot()
                log('restarted after crash')
            except Exception as e:
                finding('BOOT-FAIL', 'critical', '崩溃后重启失败', str(e))
                break
        if int(time.time() - STATE['started']) % 300 < 5:
            save_state()

    stop.set()
    log('战役结束，停止服务器')
    SRV.stop()
    save_state()
    with LOCK:
        final = dict(STATS)
    log('终态: %s' % json.dumps(final))
    log('findings=%d 详情见 findings.json' % len(FINDINGS))
    (RUNTIME / 'final.json').write_text(
        json.dumps({'stats': final, 'findings': FINDINGS}, ensure_ascii=False, indent=1),
        encoding='utf-8')


if __name__ == '__main__':
    main()
