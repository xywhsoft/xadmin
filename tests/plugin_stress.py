"""插件宿主压力战役：启停churn / 代际编译 / 失败注入 / 主脚本重载风暴。

按方案的门禁草案执行；输出各门禁的实测值。隔离夹具，回环。
"""
import ctypes, sys, subprocess, time, json, http.client, shutil, sqlite3
from ctypes import wintypes
from pathlib import Path
sys.path.insert(0, r'D:\GIT\x-admin\tests')
import smoke

PORT = 19196
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
    if not h: return (0, 0)
    try:
        pmc = PMC(); pmc.cb = ctypes.sizeof(PMC)
        rss = psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb) and pmc.WorkingSetSize or 0
        hc = wintypes.DWORD(0)
        handles = kernel32.GetProcessHandleCount(h, ctypes.byref(hc)) and hc.value or 0
        return (rss, handles)
    finally:
        kernel32.CloseHandle(h)

def req(method, path, data=None, cookie=None):
    c = http.client.HTTPConnection('127.0.0.1', PORT, timeout=20)
    h = {}
    if data is not None:
        data = json.dumps(data).encode(); h['Content-Type'] = 'application/json'
    if cookie: h['Cookie'] = cookie
    c.request(method, path, data, h)
    r = c.getresponse(); b = r.read(); c.close()
    return r.status, b

def login():
    c = http.client.HTTPConnection('127.0.0.1', PORT, timeout=20)
    c.request('POST', '/admin/login', json.dumps({'username': 'migration_smoke',
        'password': smoke.client_hash('migration_smoke', 'Temporary-test-only-9081')}).encode(),
        {'Content-Type': 'application/json'})
    r = c.getresponse(); r.read(); ck = r.getheader('Set-Cookie').split(';')[0]; c.close()
    return ck

def wait_ready(proc, port):
    for _ in range(80):
        if proc.poll() is not None: return False
        try:
            s, b = req('GET', '/admin/login')
            if s == 200: return True
        except OSError: pass
        time.sleep(0.25)
    return False

def ledger_clean(target, xid):
    with sqlite3.connect(target / 'db/main.db') as db:
        active = db.execute("SELECT COUNT(*) FROM plugin_resource WHERE xid=? AND status='active'", (xid,)).fetchone()[0]
        routes = db.execute("SELECT COUNT(*) FROM uris WHERE plugin_xid=?", (xid,)).fetchone()[0]
        menus = db.execute("SELECT COUNT(*) FROM menu WHERE plugin_xid=? AND isDelete=0", (xid,)).fetchone()[0]
    return active == 0 and routes == 0 and menus == 0

def main():
    results = {}
    target = smoke.fixture(PORT)
    log = open(target / 'stress.log', 'wb')
    proc = subprocess.Popen([r'D:\GIT\x-admin\xs.exe', str(target / 'xs.json')],
                            stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    assert wait_ready(proc, PORT), 'server not ready'
    ck = login()
    print('server pid', proc.pid)
    rss0, h0 = metrics(proc.pid)
    print(f'baseline rss={rss0//1048576}MB handles={h0}')

    # === 门禁1：100 次 enable/disable churn ===
    t0 = time.perf_counter()
    ok = True
    for i in range(100):
        s, b = req('POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, ck)
        if not json.loads(b)['result']: ok = False; break
        s, b = req('GET', '/api/plugin/hello-sdk/ping')
        if s != 200: ok = False; break
        s, b = req('POST', '/admin/plugin/disable', {'name': 'hello-sdk'}, ck)
        if not json.loads(b)['result']: ok = False; break
        if req('GET', '/api/plugin/hello-sdk/ping')[0] != 404: ok = False; break
    churn_sec = time.perf_counter() - t0
    rss1, h1 = metrics(proc.pid)
    clean = ledger_clean(target, 'hello-sdk')
    results['churn'] = dict(cycles=100 if ok else i, ok=ok, sec=round(churn_sec, 1),
                            rss_mb=rss1//1048576, handles=h1, ledger_clean=clean)
    print(f"[churn] 100 cyc {churn_sec:.0f}s rss {rss0//1048576}->{rss1//1048576}MB "
          f"hdl {h0}->{h1} ledger_clean={clean} ok={ok}")

    # === 门禁2：50 次 reload 代际 ===
    req('POST', '/admin/plugin/enable', {'name': 'hello-sdk'}, ck)
    t0 = time.perf_counter()
    ok = True
    for i in range(50):
        s, b = req('POST', '/admin/plugin/reload', {'name': 'hello-sdk'}, ck)
        if not json.loads(b)['result']: ok = False; break
    s, b = req('GET', '/api/plugin/hello-sdk/ping')
    ok = ok and s == 200
    reload_sec = time.perf_counter() - t0
    rss2, h2 = metrics(proc.pid)
    with sqlite3.connect(target / 'db/main.db') as db:
        gens = db.execute("SELECT COUNT(*), MAX(generation) FROM plugin_generation WHERE xid='hello-sdk'").fetchone()
    results['generations'] = dict(cycles=50 if ok else i, ok=ok, sec=round(reload_sec, 1),
                                  rss_mb=rss2//1048576, handles=h2, gen_rows=gens)
    print(f"[reload] 50 cyc {reload_sec:.0f}s rss {rss2//1048576}MB hdl {h2} gens={gens} ok={ok}")

    # === 门禁5：并发流量下的插件重载（GR 专项）===
    import threading
    stop_flag = threading.Event()
    errors = []
    hits = [0]
    def hammer():
        while not stop_flag.is_set():
            try:
                s, b = req('GET', '/api/plugin/hello-sdk/ping')
                if s != 200: errors.append(('ping', s))
                else: hits[0] += 1
            except Exception as e:
                errors.append((type(e).__name__, str(e)[:40]))
    threads = [threading.Thread(target=hammer) for _ in range(6)]
    for t in threads: t.start()
    ok = True
    for i in range(30):
        s, b = req('POST', '/admin/plugin/reload', {'name': 'hello-sdk'}, ck)
        if not json.loads(b)['result']: ok = False; break
    stop_flag.set()
    for t in threads: t.join()
    ok = ok and not errors and hits[0] > 100
    results['concurrent_reload'] = dict(ok=ok, pings=hits[0], errors=errors[:5])
    print(f"[concurrent-reload] 30 reloads under 6 hammer threads: {hits[0]} pings, errors={errors[:3]} ok={ok}")

    # === 门禁3：失败注入（坏 manifest / 编译错误 / OnStart 失败）===
    shutil.copytree(r'D:\GIT\x-admin\tests\plugins\hello-sdk', target / 'plugin' / 'broken-json')
    (target / 'plugin' / 'broken-json' / 'plugin.json').write_text('{ not json', encoding='utf-8')
    shutil.copytree(r'D:\GIT\x-admin\tests\plugins\hello-sdk', target / 'plugin' / 'broken-src')
    (target / 'plugin' / 'broken-src' / 'main.c').write_text(
        '#include <xs_plugin.h>\nint this is not c\n', encoding='utf-8')
    # reload 循环触发重扫（宿主扫描仅启动时）→ 直接重启服务器验证
    proc.terminate(); proc.wait(timeout=10)
    log.close(); log = open(target / 'stress.log', 'ab')
    proc = subprocess.Popen([r'D:\GIT\x-admin\xs.exe', str(target / 'xs.json')],
                            stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    assert wait_ready(proc, PORT), 'server died with broken plugins present'
    ck = login()
    s1, _ = req('POST', '/admin/plugin/enable', {'name': 'broken-json'}, ck)
    s2, _ = req('POST', '/admin/plugin/enable', {'name': 'broken-src'}, ck)
    s3, b3 = req('GET', '/api/plugin/hello-sdk/ping')  # hello-sdk 随启动自动恢复（enabled=1）
    alive = proc.poll() is None
    results['failure_injection'] = dict(host_alive=alive, hello_still_up=(s3 == 200))
    print(f"[fail-inject] host_alive={alive} hello-sdk auto-restarted={(s3==200)} "
          f"(broken-json enable:{s1} broken-src enable:{s2})")

    # === 门禁4：主脚本重载风暴 ×10（插件启用状态）===
    ok = True
    for i in range(10):
        s, b = req('POST', '/admin/tool/reload/host', cookie=ck)
        if s != 200: ok = False; break
        for _ in range(60):
            s2, _ = req('GET', '/admin/login')
            if s2 == 200: break
            time.sleep(0.1)
        # 重载后 hello-sdk 应自动重启
        for _ in range(40):
            s3, _ = req('GET', '/api/plugin/hello-sdk/ping')
            if s3 == 200: break
            time.sleep(0.1)
        if s3 != 200: ok = False; break
        ck = login()
    rss3, h3 = metrics(proc.pid)
    results['script_reload_storm'] = dict(cycles=10 if ok else i, ok=ok, rss_mb=rss3//1048576, handles=h3)
    print(f"[reload-storm] 10x ok={ok} rss {rss3//1048576}MB hdl {h3}")

    proc.terminate()
    try: proc.wait(timeout=10)
    except subprocess.TimeoutExpired: proc.kill()
    log.close()
    verdict = (results['churn']['ok'] and results['churn']['ledger_clean']
               and results['generations']['ok'] and results['failure_injection']['host_alive']
               and results['script_reload_storm']['ok'] and results['concurrent_reload']['ok'])
    print('\nSTRESS VERDICT:', 'PASS' if verdict else 'FAIL')
    Path(r'D:\GIT\x-admin\tests\.runtime\plugin_stress_result.json').write_text(
        json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    return 0 if verdict else 1

if __name__ == '__main__':
    sys.exit(main())
