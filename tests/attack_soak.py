"""Six-hour soak attack against an isolated x-admin fixture (loopback only).

Mixed legitimate load + attack replay + hot-reload stress, with process
memory/handle sampling to surface leaks. Never touches the root database;
asserts its hash at exit. Workers use keep-alive connections (the first
launch burned ~16k ephemeral ports in 55s with per-request connects).
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import http.client
import json
import random
import socket
import subprocess
import sys
import threading
import time
from ctypes import wintypes

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ADMIN = 'migration_smoke'
PASSWORD = 'Temporary-test-only-9081'

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)


class PMC(ctypes.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD),
                ('PeakWorkingSetSize', ctypes.c_size_t), ('WorkingSetSize', ctypes.c_size_t),
                ('QuotaPeakPagedPoolUsage', ctypes.c_size_t), ('QuotaPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t), ('QuotaNonPagedPoolUsage', ctypes.c_size_t),
                ('PagefileUsage', ctypes.c_size_t), ('PeakPagefileUsage', ctypes.c_size_t)]


def process_metrics(pid):
    h = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)
    if not h:
        return {}
    try:
        pmc = PMC()
        pmc.cb = ctypes.sizeof(PMC)
        rss = handles = None
        if psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb):
            rss = pmc.WorkingSetSize
        hc = wintypes.DWORD(0)
        if kernel32.GetProcessHandleCount(h, ctypes.byref(hc)):
            handles = hc.value
        return {'rss': rss, 'handles': handles}
    finally:
        kernel32.CloseHandle(h)


def one_shot(port, method, path, data=None, cookie=None, timeout=10):
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=timeout)
    headers = {}
    body = None
    if data is not None:
        body = json.dumps(data).encode() if not isinstance(data, bytes) else data
        headers['Content-Type'] = 'application/json'
    if cookie:
        headers['Cookie'] = cookie
    try:
        conn.request(method, path, body, headers)
        resp = conn.getresponse()
        return resp.status, dict(resp.getheaders()), resp.read()
    finally:
        conn.close()


class Soak:
    def __init__(self, port, hours, outdir):
        self.port = port
        self.deadline = time.time() + hours * 3600
        self.outdir = Path(outdir)
        self.outdir.mkdir(parents=True, exist_ok=True)
        self.metrics_path = self.outdir / 'soak_metrics.jsonl'
        self.latencies = []
        self.errors = []
        self.stop = threading.Event()
        self.server_died = False
        self.counters = {'requests': 0, 'http_errors': 0, 'exceptions': 0, 'reloads': 0,
                         'login_ok': 0, 'crud_ok': 0}
        self.lock = threading.Lock()

    def note_latency(self, elapsed, status):
        with self.lock:
            self.counters['requests'] += 1
            self.latencies.append(elapsed)
            if status >= 500:
                self.counters['http_errors'] += 1

    def note_error(self, method, path, exc):
        with self.lock:
            self.counters['exceptions'] += 1
            self.errors.append(f'{method} {path[:70]}: {type(exc).__name__}')
            if len(self.errors) > 400:
                del self.errors[:200]

    def admin_login(self):
        status, headers, body = one_shot(self.port, 'POST', '/admin/login', {
            'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)})
        if status != 200 or not json.loads(body)['result']:
            raise RuntimeError(f'admin login failed: {status}')
        return headers['Set-Cookie'].split(';')[0]

    def worker(self, wid):
        rng = random.Random(1000 + wid)
        cookie = None
        seq = 0
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        attacks = [
            ('GET', '/../db/main.db', None),
            ('GET', '/%2e%2e/options/global.json', None),
            ('GET', '/admin/auth/user?search=%25%25%25', None),
            ('POST', '/admin/login', b'{not json'),
            ('GET', '/admin/auth/user?page=-1&limit=-5', None),
            ('GET', '/admin/option/file?file=..%2F..%2F..%2Fwindows%2Fwin.ini', None),
            ('GET', '/admin/form?file=CON.json', None),
            ('POST', '/api/v1/register', {'username': 'x' * 4000, 'password': 'y'}),
            ('GET', '/admin/nonexistent/%s%s%s%n', None),
        ]
        reads = ['/admin/auth/user?page=1&limit=10', '/admin/auth/role?page=1&limit=10',
                 '/admin/member/user?page=1&limit=10', '/admin/logs?page=1&limit=10',
                 '/admin/menu', '/admin/option/files', '/admin/trace', '/admin/trace/route',
                 '/admin/view/auth/user/add', '/layui/layui.js']

        def req(method, path, data=None, ck=None):
            nonlocal conn
            headers = {}
            body = None
            if data is not None:
                body = json.dumps(data).encode() if not isinstance(data, bytes) else data
                headers['Content-Type'] = 'application/json'
            if ck:
                headers['Cookie'] = ck
            t0 = time.perf_counter()
            try:
                conn.request(method, path, body, headers)
                resp = conn.getresponse()
                payload = resp.read()
                status = resp.status
                hdrs = dict(resp.getheaders())
            except Exception as exc:  # noqa: BLE001
                try:
                    conn.close()
                except Exception:  # noqa: BLE001
                    pass
                conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
                self.note_error(method, path, exc)
                raise
            finally:
                self.note_latency((time.perf_counter() - t0) * 1000,
                                  status if 'status' in dir() else 0)
            return status, hdrs, payload

        while not self.stop.is_set() and time.time() < self.deadline:
            seq += 1
            try:
                if cookie is None:
                    cookie = self.admin_login()
                    with self.lock:
                        self.counters['login_ok'] += 1
                roll = rng.random()
                if roll < 0.55:
                    req('GET', rng.choice(reads), ck=cookie)
                elif roll < 0.70:
                    name = f'soak_{wid}_{seq}'
                    status, _, body = req('POST', '/admin/auth/role',
                                          {'name': name, 'desc': 'soak', 'authList': '[]',
                                           'authLevel': 0}, cookie)
                    rid = None
                    try:
                        rid = json.loads(body).get('data', {}).get('id') if status == 200 else None
                    except Exception:  # noqa: BLE001
                        pass
                    if rid:
                        with self.lock:
                            self.counters['crud_ok'] += 1
                        req('PUT', '/admin/auth/role',
                            {'id': rid, 'name': name, 'desc': 'soak2',
                             'authList': '[]', 'authLevel': 1}, cookie)
                        req('GET', f'/admin/auth/role?search={name}', ck=cookie)
                        if seq % 3 == 0:
                            req('DELETE', f'/admin/auth/role?id={rid}', ck=cookie)
                elif roll < 0.78:
                    m = f'soakm_{wid}_{seq}'
                    one_shot(self.port, 'POST', '/api/v1/register', {
                        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
                    s3, h3, _ = one_shot(self.port, 'POST', '/api/v1/login', {
                        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
                    if s3 == 200:
                        mc = h3['Set-Cookie'].split(';')[0]
                        one_shot(self.port, 'GET', '/api/v1/profile', cookie=mc)
                        one_shot(self.port, 'PUT', '/api/v1/profile',
                                 {'nickname': f'昵称{seq}'}, cookie=mc)
                        one_shot(self.port, 'POST', '/api/v1/logout', cookie=mc)
                elif roll < 0.86:
                    status, _, body = req('GET', '/admin/option?file=example.json', ck=cookie)
                    try:
                        values = json.loads(body)['data']['values']
                        values['input_int'] = seq % 1000
                        req('POST', '/admin/option', {
                            'file': 'example.json', 'source': 'option', 'data': values}, cookie)
                    except Exception:  # noqa: BLE001
                        pass
                elif roll < 0.94:
                    method, path, payload = rng.choice(attacks)
                    req(method, path, payload, ck=cookie)
                else:
                    req('GET', '/admin/logout', ck=cookie)
                    cookie = None
            except Exception:  # noqa: BLE001
                if rng.random() < 0.3:
                    cookie = None
            time.sleep(rng.uniform(0.005, 0.05))

    def monitor(self, target, pid, proc):
        metrics_file = open(self.metrics_path, 'a', encoding='utf-8')
        last_reload = time.time()
        start = time.time()
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
                self.server_died = True
                self.stop.set()
            with self.lock:
                lat = sorted(self.latencies)
                n = len(lat)
                snap_lat = {'p50': round(lat[n // 2], 1) if n else None,
                            'p95': round(lat[int(n * 0.95)], 1) if n else None,
                            'max': round(lat[-1], 1) if n else None,
                            'count': n}
                self.latencies = []
                counters = dict(self.counters)
                self.counters['requests'] = 0
                self.counters['http_errors'] = 0
                errs = list(self.errors[-3:])
            pm = process_metrics(pid)
            db_bytes = (target / 'db/main.db').stat().st_size
            metrics_file.write(json.dumps({
                'elapsed_min': round((time.time() - start) / 60, 1),
                'interval': interval, 'alive': alive, 'responsive': responsive,
                'proc': pm, 'db_bytes': db_bytes, 'lat': snap_lat,
                'counters': counters, 'recent_errors': errs,
            }, ensure_ascii=False) + '\n')
            metrics_file.flush()
            if alive and responsive and time.time() - last_reload >= 1800:
                last_reload = time.time()
                try:
                    cookie = self.admin_login()
                    status, _, _ = one_shot(self.port, 'POST', '/admin/tool/reload/host',
                                            cookie=cookie)
                    with self.lock:
                        self.counters['reloads'] += 1
                    metrics_file.write(json.dumps(
                        {'reload_submitted': status, 'at_interval': interval}) + '\n')
                except Exception as exc:  # noqa: BLE001
                    metrics_file.write(json.dumps({'reload_failed': str(exc)[:200]}) + '\n')
                metrics_file.flush()
        metrics_file.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19100)
    parser.add_argument('--hours', type=float, default=6.0)
    parser.add_argument('--workers', type=int, default=8)
    args = parser.parse_args()

    root_hash = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest()
    target = smoke.fixture(args.port)
    log = open(target / 'soak_server.log', 'ab')
    proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                            stdout=log, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    soak = Soak(args.port, args.hours, target)
    print(f'soak fixture={target} pid={proc.pid} hours={args.hours}', flush=True)

    threads = [threading.Thread(target=soak.worker, args=(i,), daemon=True)
               for i in range(args.workers)]
    mon = threading.Thread(target=soak.monitor, args=(target, proc.pid, proc), daemon=True)
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

    summary = {
        'server_alive_at_end': alive,
        'server_died': soak.server_died,
        'exit_code': proc.poll(),
        'root_db_unchanged':
            hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest() == root_hash,
        'metrics_file': str(soak.metrics_path),
        'error_tail': soak.errors[-20:],
    }
    (target / 'soak_summary.json').write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
