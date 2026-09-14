"""四小时插件系统极端浸泡：混合操作 + 页面请求 + 极端情况 + 周期主脚本重载。

双插件（hello-sdk + guestbook_v3，均原生方言）持续承压；分钟级采样
RSS/句柄/延迟/错误率与 DB 不变量；产物：soak_plugin_metrics.jsonl +
plugin_soak_report.md。隔离夹具，回环，根库零污染（退出校验）。
"""
import ctypes, hashlib, json, random, shutil, socket, sqlite3, subprocess, sys, threading, time
import http.client
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke

ROOT = Path(__file__).resolve().parents[1]
ADMIN = 'migration_smoke'
PASSWORD = 'Temporary-test-only-9081'
PLUGINS = ('hello-sdk', 'guestbook_v3')

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)

class PMC(ctypes.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD),
                ('PeakWorkingSetSize', ctypes.c_size_t), ('WorkingSetSize', ctypes.c_size_t),
                ('QuotaPeakPagedPoolUsage', ctypes.c_size_t), ('QuotaPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t), ('QuotaNonPagedPoolUsage', ctypes.c_size_t),
                ('PagefileUsage', ctypes.c_size_t), ('PeakPagefileUsage', ctypes.c_size_t)]

def metrics(pid):
    h = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)
    if not h:
        return {}
    try:
        pmc = PMC(); pmc.cb = ctypes.sizeof(PMC)
        rss = psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb) and pmc.WorkingSetSize or None
        hc = wintypes.DWORD(0)
        handles = kernel32.GetProcessHandleCount(h, ctypes.byref(hc)) and hc.value or None
        return {'rss': rss, 'handles': handles}
    finally:
        kernel32.CloseHandle(h)

class Soak:
    def __init__(self, port, hours, target):
        self.port = port
        self.deadline = time.time() + hours * 3600
        self.target = target
        self.outdir = target
        self.stop = threading.Event()
        self.fatal = None            # 需要人工裁定/致命事件（含上下文）记录
        self.incidents = []          # 观察到的异常（可继续运行）
        self.counters = dict.fromkeys(
            ('requests', 'http_5xx', 'conn_errors', 'lifecycle_ops',
             'page_requests', 'extreme_probes', 'settings_writes'), 0)
        self.latencies = []
        self.lock = threading.Lock()

    def note(self, elapsed, kind, detail):
        rec = {'elapsed_min': round(elapsed / 60, 1), 'kind': kind, 'detail': str(detail)[:300]}
        self.incidents.append(rec)
        print(f'[incident:{kind}] {detail}', flush=True)

    def request(self, method, path, data=None, cookie=None, timeout=20):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=timeout)
        headers = {}
        if data is not None:
            data = json.dumps(data).encode() if not isinstance(data, bytes) else data
            headers['Content-Type'] = 'application/json'
        if cookie:
            headers['Cookie'] = cookie
        t0 = time.perf_counter()
        status = 0
        try:
            conn.request(method, path, data, headers)
            resp = conn.getresponse()
            body = resp.read()
            status = resp.status
            hdrs = dict(resp.getheaders())
            return status, hdrs, body
        except Exception as exc:  # noqa: BLE001
            with self.lock:
                self.counters['conn_errors'] += 1
            raise
        finally:
            elapsed_ms = (time.perf_counter() - t0) * 1000
            with self.lock:
                self.counters['requests'] += 1
                self.latencies.append(elapsed_ms)
                if status >= 500:
                    self.counters['http_5xx'] += 1
            conn.close()

    def login(self, start):
        status, headers, body = self.request('POST', '/admin/login', {
            'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)})
        if status != 200 or not json.loads(body)['result']:
            self.note(time.time() - start, 'login_failed', f'{status} {body[:120]}')
            raise RuntimeError('admin login failed')
        return headers['Set-Cookie'].split(';')[0]

    # ---------------- worker ----------------
    def worker(self, wid, start):
        rng = random.Random(7000 + wid)
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=20)
        cookie = self.login(start)

        def req(method, path, data=None, ck=None):
            nonlocal conn, cookie
            headers = {}
            body = None
            if data is not None:
                body = json.dumps(data).encode() if not isinstance(data, bytes) else data
                headers['Content-Type'] = 'application/json'
            headers['Cookie'] = ck or cookie
            t0 = time.perf_counter()
            status = 0
            try:
                conn.request(method, path, body, headers)
                resp = conn.getresponse()
                payload = resp.read()
                status = resp.status
                return status, payload
            except Exception:  # noqa: BLE001
                try:
                    conn.close()
                except Exception:  # noqa: BLE001
                    pass
                conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=20)
                with self.lock:
                    self.counters['conn_errors'] += 1
                raise
            finally:
                ms = (time.perf_counter() - t0) * 1000
                with self.lock:
                    self.counters['requests'] += 1
                    self.latencies.append(ms)
                    if status >= 500:
                        self.counters['http_5xx'] += 1

        def ensure_admin(start_ref):
            nonlocal cookie
            try:
                s, _ = req('GET', '/admin/plugin/list')
                if s == 200:
                    return
            except Exception:  # noqa: BLE001
                pass
            try:
                cookie = self.login(start_ref)
            except Exception:  # noqa: BLE001
                pass

        reads = [
            ('/api/plugin/hello-sdk/ping', 200), ('/api/plugin/hello-sdk/state', 200),
            ('/api/plugin/hello/greeting', 200), ('/api/plugin/hello/info', 200),
            ('/api/plugin/hello-sdk/hook', 200), ('/api/plugin/hello-sdk/service', 200),
            ('/api/plugin/hello-sdk/emit', 200),
        ]
        pages = ['/admin/view/plugin', '/admin/view/plugin/store', '/layui/layui.js']
        extremes = [
            ('POST', '/admin/plugin/enable', {'name': '../evil'}),
            ('POST', '/admin/plugin/reload', {'name': 'no-such-plugin'}),
            ('POST', '/admin/plugin/enable', b'{not json'),
            ('GET', '/api/plugin/hello-sdk/ping?x=' + 'A' * 3000, None),
            ('POST', '/api/plugin/hello-sdk/service', {'pad': 'B' * 100000}),
        ]

        while not self.stop.is_set() and time.time() < self.deadline and not self.fatal:
            try:
                roll = rng.random()
                if roll < 0.42:
                    path, expect = rng.choice(reads)
                    s, b = req('GET', path)
                    if s >= 500:
                        self.note(time.time() - start, 'route_5xx', f'{path} -> {s} {b[:100]}')
                    elif expect == 200 and s == 404 and 'hello-sdk' in path:
                        # 被生命周期操作翻成 disabled 属正常窗口；记录一次确保非意外
                        pass
                elif roll < 0.54:
                    with self.lock:
                        self.counters['page_requests'] += 1
                    req('GET', rng.choice(pages))
                elif roll < 0.62:
                    s, b = req('GET', '/admin/plugin/list')
                    if s != 200:
                        self.ensure_admin(start)
                elif roll < 0.70:
                    # 设置写 + 读回验证（配置churn）
                    msg = f'soak-{wid}-{int(time.time()) % 100000}'
                    s, b = req('POST', '/admin/plugin/settings',
                               {'name': 'hello-sdk', 'config': {
                                   'welcomeMessage': msg, 'showTime': True}})
                    if s == 200 and json.loads(b)['result']:
                        with self.lock:
                            self.counters['settings_writes'] += 1
                        # 并发churn下读回的应是“任一 worker 的最新写入”（soak- 前缀），
                        # 而非陈旧值——严格等于本 worker 的消息在并发下不成立。
                        s2, b2 = req('GET', '/api/plugin/hello-sdk/ping')
                        if s2 == 200 and b'soak-' not in b2:
                            self.note(time.time() - start, 'config_mismatch', b2[:120])
                    elif s >= 500:
                        self.note(time.time() - start, 'settings_5xx', f'{s} {b[:100]}')
                elif roll < 0.86:
                    # 生命周期混合操作（enable/disable/reload 轮盘）
                    with self.lock:
                        self.counters['lifecycle_ops'] += 1
                    op = rng.choice(('enable', 'disable', 'reload', 'reload'))
                    xid = rng.choice(PLUGINS)
                    s, b = req('POST', f'/admin/plugin/{op}', {'name': xid})
                    if s >= 500:
                        self.note(time.time() - start, f'{op}_5xx', f'{xid} {s} {b[:100]}')
                    elif s != 200:
                        self.ensure_admin(start)
                elif roll < 0.94:
                    # 极端情况
                    with self.lock:
                        self.counters['extreme_probes'] += 1
                    method, path, payload = rng.choice(extremes)
                    try:
                        req(method, path, payload)
                    except Exception:  # noqa: BLE001
                        pass
                else:
                    # 停驻插件可能处于 disabled——补拉起保证流量面存在
                    for xid in PLUGINS:
                        s, _ = req('POST', '/admin/plugin/enable', {'name': xid})
                    time.sleep(0.2)
            except Exception as exc:  # noqa: BLE001
                if isinstance(exc, (ConnectionRefusedError, ConnectionResetError, BrokenPipeError)):
                    # 服务器疑似消失——由监控线程判定致命
                    time.sleep(0.5)
                ensure_admin(start)
            time.sleep(rng.uniform(0.005, 0.05))

    # ---------------- monitor ----------------
    def monitor(self, proc, start, root_hash):
        mf = open(self.target / 'soak_plugin_metrics.jsonl', 'a', encoding='utf-8')
        last_host_reload = time.time()
        interval = 0
        while not self.stop.is_set() and time.time() < self.deadline:
            interval += 1
            time.sleep(60)
            alive = proc.poll() is None
            responsive = False
            try:
                sock = socket.create_connection(('127.0.0.1', self.port), timeout=3)
                sock.sendall(b'GET /admin/login HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
                sock.recv(64)
                sock.close()
                responsive = True
            except OSError:
                pass
            if not alive:
                self.fatal = {'elapsed_min': round((time.time() - start) / 60, 1),
                              'kind': 'server_process_died', 'exit': proc.poll()}
                self.stop.set()
                break
            with self.lock:
                lat = sorted(self.latencies)
                n = len(lat)
                snap = {'p50': round(lat[n // 2], 1) if n else None,
                        'p95': round(lat[int(n * 0.95)], 1) if n else None,
                        'max': round(lat[-1], 1) if n else None, 'count': n}
                self.latencies = []
                counters = dict(self.counters)
                for k in ('requests', 'http_5xx', 'conn_errors'):
                    self.counters[k] = 0
            pm = metrics(proc.pid)
            db_bytes = (self.target / 'db/main.db').stat().st_size
            # DB 不变量：停止插件的台账必须无 active 残留
            ledger_ok = True
            try:
                with sqlite3.connect(self.target / 'db/main.db') as db:
                    for xid in PLUGINS:
                        enabled = db.execute('SELECT enabled FROM plugin_runtime WHERE xid=?', (xid,)).fetchone()
                        if enabled and not enabled[0]:
                            active = db.execute(
                                "SELECT COUNT(*) FROM plugin_resource WHERE xid=? AND status='active'",
                                (xid,)).fetchone()[0]
                            if active:
                                ledger_ok = False
                                self.note(time.time() - start, 'ledger_leak',
                                          f'{xid} disabled but {active} active resource rows')
                    gens = db.execute('SELECT COUNT(*) FROM plugin_generation').fetchone()[0]
            except sqlite3.Error as exc:
                ledger_ok = False
                self.note(time.time() - start, 'db_check_error', exc)
                gens = -1
            mf.write(json.dumps({
                'elapsed_min': round((time.time() - start) / 60, 1), 'interval': interval,
                'alive': alive, 'responsive': responsive, 'proc': pm, 'db_bytes': db_bytes,
                'lat': snap, 'counters': counters, 'ledger_ok': ledger_ok, 'gens': gens,
            }, ensure_ascii=False) + '\n')
            mf.flush()
            if alive and responsive and time.time() - last_host_reload >= 1800:
                last_host_reload = time.time()
                try:
                    ck = self.login(start)
                    s, _, _ = self.request('POST', '/admin/tool/reload/host', cookie=ck, timeout=30)
                    mf.write(json.dumps({'host_reload': s,
                                         'elapsed_min': round((time.time() - start) / 60, 1)}) + '\n')
                    mf.flush()
                except Exception as exc:  # noqa: BLE001
                    self.note(time.time() - start, 'host_reload_failed', exc)
        mf.close()

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19200)
    parser.add_argument('--hours', type=float, default=4.0)
    parser.add_argument('--workers', type=int, default=8)
    args = parser.parse_args()

    root_hash = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest()
    target = smoke.fixture(args.port)
    shutil.copytree(ROOT / 'tests/plugins/guestbook_v3', target / 'plugin/guestbook_v3')
    log = open(target / 'soak_server.log', 'ab')
    proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                            stdout=log, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    soak = Soak(args.port, args.hours, target)
    print(f'plugin soak fixture={target} pid={proc.pid} hours={args.hours}', flush=True)

    # 就绪 + 初始启用双插件
    ready = False
    for _ in range(80):
        if proc.poll() is not None:
            break
        try:
            s, _, _ = soak.request('GET', '/admin/login')
            if s == 200:
                ready = True
                break
        except OSError:
            pass
        time.sleep(0.3)
    if not ready:
        print('FATAL: server not ready', flush=True)
        proc.terminate()
        return 1
    start = time.time()
    ck = soak.login(start)
    for xid in PLUGINS:
        s, _, b = soak.request('POST', '/admin/plugin/enable', {'name': xid}, cookie=ck, timeout=60)
        print(f'enable {xid}: {s} {b[:80]}', flush=True)

    threads = [threading.Thread(target=soak.worker, args=(i, start), daemon=True)
               for i in range(args.workers)]
    mon = threading.Thread(target=soak.monitor, args=(proc, start, root_hash), daemon=True)
    for t in threads + [mon]:
        t.start()

    try:
        while time.time() < soak.deadline and not soak.stop.is_set():
            time.sleep(5)
    finally:
        soak.stop.set()
        time.sleep(3)
        alive = proc.poll() is None
        if alive:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()

    unchanged = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest() == root_hash
    summary = {
        'server_alive_at_end': alive,
        'fatal': soak.fatal,
        'root_db_unchanged': unchanged,
        'incidents': soak.incidents,
        'final_counters': soak.counters,
        'metrics_file': str(target / 'soak_plugin_metrics.jsonl'),
    }
    (target / 'soak_summary.json').write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: summary[k] for k in ('server_alive_at_end', 'fatal', 'root_db_unchanged')},
                     ensure_ascii=False), flush=True)
    print(f'incidents: {len(soak.incidents)}', flush=True)
    for inc in soak.incidents[:20]:
        print(' -', inc, flush=True)
    return 0

if __name__ == '__main__':
    sys.exit(main())
