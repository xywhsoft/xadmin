# -*- coding: utf-8 -*-
"""10 小时全接口压测 + 渗透战役编排器（loopback 隔离夹具）。

阶段：
  A  全路由性能扫描（逐端点 1/16 并发梯度，p50/p95/p99/eps）
  B  新家族定向渗透（attachment 上传/购买/防盗链/路径穿越/multipart 模糊，
     sched cron 模糊/导入混淆/字段滥用，notify 大参数/ID 爆炸）
  C  经典渗透复刻（会话/注入/越权/资源/逻辑/泄露）
  D  持续混合浸泡（高压混合负载 + 主脚本重载 + 插件启停，资源漂移监测）

产出：campaign_metrics.jsonl（逐端点采样）、campaign_findings.json、
campaign_report.md（终稿）、campaign_state.json（断点/进度，供巡检）。
只攻击一次性夹具；根库与仓库只读。"""
from pathlib import Path
import hashlib
import http.client
import json
import os
import socket
import sqlite3
import statistics
import subprocess
import sys
import threading
import time
import traceback
import uuid
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'tests' / '.runtime' / 'campaign10h'
ADMIN = 'migration_smoke'
PASSWORD = 'Temporary-test-only-9081'
PORT = 19410
DURATION_HOURS = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0

FINDINGS = []
STATE = {'phase': 'boot', 'started': time.time(), 'fixed_and_restarted': 0}


def finding(fid, severity, title, detail):
    FINDINGS.append({'id': fid, 'severity': severity, 'title': title,
                     'detail': detail, 'at': time.strftime('%H:%M:%S')})
    print('[%s] %s: %s\n    %s' % (severity, fid, title, detail), flush=True)
    save_findings()


def save_findings():
    (RUNTIME / 'campaign_findings.json').write_text(
        json.dumps(FINDINGS, ensure_ascii=False, indent=1), encoding='utf-8')


def save_state():
    STATE['elapsed_min'] = round((time.time() - STATE['started']) / 60, 1)
    (RUNTIME / 'campaign_state.json').write_text(
        json.dumps(STATE, ensure_ascii=False, indent=1), encoding='utf-8')


def metrics_row(phase, endpoint, conc, latencies, eps, extra=None):
    row = {'t': round(time.time(), 3), 'phase': phase, 'endpoint': endpoint,
           'conc': conc, 'n': len(latencies),
           'p50_ms': round(statistics.median(latencies) * 1000, 2) if latencies else None,
           'p95_ms': round(sorted(latencies)[int(len(latencies) * 0.95) - 1] * 1000, 2) if latencies else None,
           'p99_ms': round(sorted(latencies)[int(len(latencies) * 0.99) - 1] * 1000, 2) if len(latencies) >= 100 else None,
           'max_ms': round(max(latencies) * 1000, 2) if latencies else None,
           'eps': round(eps, 1)}
    if extra:
        row.update(extra)
    with open(RUNTIME / 'campaign_metrics.jsonl', 'a', encoding='utf-8') as f:
        f.write(json.dumps(row, ensure_ascii=False) + '\n')


class Client:
    def __init__(self, port, timeout=20):
        self.port = port
        self.timeout = timeout

    def request(self, method, path, data=None, cookie=None, ctype=None, raw_headers=None, timeout=None, retry=True):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=timeout or self.timeout)
        headers = dict(raw_headers or {})
        if data is not None:
            data = json.dumps(data).encode() if not isinstance(data, bytes) else data
            headers.setdefault('Content-Type', ctype or 'application/json')
        if cookie:
            headers['Cookie'] = cookie
        t0 = time.perf_counter()
        try:
            conn.request(method, path, body=data, headers=headers)
            r = conn.getresponse()
            body = r.read()
            lat = time.perf_counter() - t0
            return r.status, dict(r.getheaders()), body, lat
        except (OSError, http.client.HTTPException) as e:
            if retry and time.perf_counter() - t0 < 25:
                time.sleep(0.5)
                return self.request(method, path, data, cookie, ctype, raw_headers, timeout, False)
            return 0, {}, str(type(e).__name__).encode(), time.perf_counter() - t0
        finally:
            conn.close()

    def raw(self, payload, read_bytes=8192, timeout=10):
        sock = socket.create_connection(('127.0.0.1', self.port), timeout=timeout)
        try:
            sock.sendall(payload)
            chunks = []
            deadline = time.time() + timeout
            while time.time() < deadline:
                data = sock.recv(65536)
                if not data:
                    break
                chunks.append(data)
                if sum(len(c) for c in chunks) >= read_bytes:
                    break
            return b''.join(chunks)
        finally:
            sock.close()


def process_stats(pid):
    try:
        out = subprocess.run(['powershell', '-NoProfile', '-Command',
                              '(Get-Process -Id %d).WorkingSet64; '
                              '(Get-Process -Id %d).HandleCount' % (pid, pid)],
                             capture_output=True, text=True, timeout=15).stdout.split()
        return int(out[0]) // 1048576, int(out[1])
    except Exception:
        return -1, -1


def multipart_body(fields, file_field, filename, content, boundary=None):
    b = (boundary or uuid.uuid4().hex[:16]).encode()
    body = b''
    for k, v in fields.items():
        body += (b'--' + b + b'\r\nContent-Disposition: form-data; name="' + k.encode() +
                 b'"\r\n\r\n' + (v if isinstance(v, bytes) else str(v).encode()) + b'\r\n')
    if file_field:
        body += (b'--' + b + b'\r\nContent-Disposition: form-data; name="' + file_field.encode() +
                 b'"; filename="' + (filename if isinstance(filename, bytes) else filename.encode()) +
                 b'"\r\nContent-Type: application/octet-stream\r\n\r\n' + content + b'\r\n')
    body += b'--' + b + b'--\r\n'
    return 'multipart/form-data; boundary=' + (boundary or b.decode()), body


def jload(body):
    try:
        return json.loads(body)
    except Exception:
        return None


class Server:
    def __init__(self):
        self.target = None
        self.proc = None
        self.pid = None

    def boot(self):
        self.target = smoke.fixture(PORT)
        log = open(self.target / 'campaign.log', 'wb')
        self.proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(self.target / 'xs.json')],
                                     cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
        self.pid = self.proc.pid
        c = Client(PORT, timeout=30)
        for _ in range(120):
            if self.proc.poll() is not None:
                raise RuntimeError('server died at boot: ' +
                    open(self.target / 'campaign.log', errors='replace').read()[-2000:])
            try:
                if c.request('GET', '/admin/login')[0] == 200:
                    return
            except OSError:
                pass
            time.sleep(0.25)
        raise RuntimeError('server not ready')

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=20)
            except Exception:
                self.proc.kill()


def admin_cookie(c):
    s, h, b, _ = c.request('POST', '/admin/login', {
        'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)})
    assert jload(b) and jload(b).get('result'), b
    return h['Set-Cookie'].split(';')[0]


def member_cookie(c, name, deposit=None):
    payload = {'username': name, 'password': smoke.client_hash(name, PASSWORD)}
    c.request('POST', '/api/v1/register', payload)
    s, h, b, _ = c.request('POST', '/api/v1/login', payload)
    ck = h.get('Set-Cookie', '').split(';')[0]
    if deposit:
        with sqlite3.connect(SRV.target / 'db/main.db') as db:
            db.execute('UPDATE member SET balance = ? WHERE username = ?', (deposit, name))
            db.commit()
    return ck


# ==================== 阶段 A：全路由性能扫描 ====================

def phase_a_sweep(c, ck, mck):
    """逐端点梯度并发压测。GET 只读端点 200 请求；写端点用安全循环。"""
    STATE['phase'] = 'A-perf-sweep'
    save_state()
    endpoints = [
        # (label, method, path, body, cookie)
        ('GET /admin/login', 'GET', '/admin/login', None, None),
        ('GET / (index)', 'GET', '/', None, None),
        ('GET /admin', 'GET', '/admin', None, ck),
        ('GET /admin/menu', 'GET', '/admin/menu', None, ck),
        ('GET /admin/view/home', 'GET', '/admin/view/home', None, ck),
        ('GET /admin/view/auth/user', 'GET', '/admin/view/auth/user', None, ck),
        ('GET /admin/auth/user list', 'GET', '/admin/auth/user?page=1&limit=10', None, ck),
        ('GET /admin/auth/role list', 'GET', '/admin/auth/role?page=1&limit=10', None, ck),
        ('GET /admin/auth/group list', 'GET', '/admin/auth/group?page=1&limit=10', None, ck),
        ('GET /admin/auth/auth list', 'GET', '/admin/auth/auth?page=1&limit=10', None, ck),
        ('GET /admin/auth/uris list', 'GET', '/admin/auth/uris?page=1&limit=10', None, ck),
        ('GET /admin/member/user list', 'GET', '/admin/member/user?page=1&limit=10', None, ck),
        ('GET /admin/member/group list', 'GET', '/admin/member/group?page=1&limit=10', None, ck),
        ('GET /admin/member/authgroup', 'GET', '/admin/member/authgroup?page=1&limit=10', None, ck),
        ('GET /admin/member/auth', 'GET', '/admin/member/auth?page=1&limit=10', None, ck),
        ('GET /admin/logs', 'GET', '/admin/logs?page=1&limit=10', None, ck),
        ('GET /admin/option/get', 'GET', '/admin/option/get?file=global.json', None, ck),
        ('GET /admin/option/files', 'GET', '/admin/option/files', None, ck),
        ('GET /admin/form/list', 'GET', '/admin/form/list', None, ck),
        ('GET /admin/trace/overview', 'GET', '/admin/trace/overview', None, ck),
        ('GET /admin/trace/session', 'GET', '/admin/trace/session', None, ck),
        ('GET /admin/trace/option', 'GET', '/admin/trace/option', None, ck),
        ('GET /admin/trace/auth', 'GET', '/admin/trace/auth', None, ck),
        ('GET /admin/trace/route', 'GET', '/admin/trace/route', None, ck),
        ('GET /admin/plugin/list', 'GET', '/admin/plugin/list', None, ck),
        ('GET /admin/sched/tasks', 'GET', '/admin/sched/tasks?page=1&limit=10', None, ck),
        ('GET /admin/sched/dashboard', 'GET', '/admin/sched/dashboard', None, ck),
        ('GET /admin/sched/logs', 'GET', '/admin/sched/logs?page=1&limit=10', None, ck),
        ('GET /admin/attachment/list', 'GET', '/admin/attachment/list?page=1&limit=10', None, ck),
        ('GET /admin/attachment/stats', 'GET', '/admin/attachment/stats', None, ck),
        ('GET /admin/member/notify list', 'GET', '/admin/member/notify?page=1&limit=10', None, ck),
        ('GET /api/v1/profile', 'GET', '/api/v1/profile', None, mck),
        ('GET /api/v1/balance', 'GET', '/api/v1/balance', None, mck),
        ('GET /api/v1/balance/log', 'GET', '/api/v1/balance/log', None, mck),
        ('GET /api/v1/notify/list', 'GET', '/api/v1/notify/list', None, mck),
        ('GET /api/v1/notify/unread_count', 'GET', '/api/v1/notify/unread_count', None, mck),
        ('GET /api/v1/attachment/my', 'GET', '/api/v1/attachment/my', None, mck),
        ('GET /api/v1/attachment/purchased', 'GET', '/api/v1/attachment/purchased', None, mck),
        ('GET /brand/admin', 'GET', '/brand/admin', None, None),
    ]
    for label, method, path, body, cookie in endpoints:
        for conc in (1, 16):
            lat = []
            errs = 0
            t0 = time.perf_counter()

            def worker():
                cc = Client(PORT, timeout=30)
                ls = []
                es = 0
                for _ in range(max(1, 200 // conc)):
                    s, _, _, dt = cc.request(method, path, body, cookie)
                    ls.append(dt)
                    if s >= 500 or s == 0:
                        es += 1
                return ls, es

            with ThreadPoolExecutor(conc) as ex:
                for ls, es in ex.map(lambda _: worker(), range(conc)):
                    lat.extend(ls)
                    errs += es
            elapsed = time.perf_counter() - t0
            metrics_row('A', label, conc, lat, len(lat) / elapsed, {'errors': errs})
        save_state()
    finding('A-done', 'info', '阶段A完成', '全路由性能扫描 %d 端点' % len(endpoints))


# ==================== 阶段 B：新家族定向渗透 ====================

def phase_b_attachment(c, ck, mck):
    STATE['phase'] = 'B-attachment'
    save_state()

    # B1: 上传扩展名约束（配置允许表外的可执行扩展）
    for evil in ('exe', 'bat', 'php', 'asp', 'jsp', 'html'):
        ctype, body = multipart_body({}, 'file', 'x.' + evil, b'MZ')
        s, _, b_, _ = c.request('POST', '/admin/attachment/upload', body, ck, ctype)
        if jload(b_) and jload(b_).get('result'):
            finding('ATT-1', 'high', '危险扩展名上传成功: ' + evil, '后台可上传 %s' % evil)

    # B2: 文件名路径穿越（multipart filename 含 ../ 与绝对路径）
    for fname in ('../../evil.zip', '../..\\evil.zip', '/abs/evil.zip', '.htaccess'):
        ctype, body = multipart_body({'modelName': 'probe'}, 'file', fname, b'x' * 10)
        s, _, b_, _ = c.request('POST', '/admin/attachment/upload', body, ck, ctype)
        if jload(b_) and jload(b_).get('result'):
            xid = jload(b_)['data']['xid']
            with sqlite3.connect(SRV.target / 'db/main.db') as db:
                path = db.execute('SELECT path FROM attachment WHERE xid=?', (xid,)).fetchone()
            uploads = (SRV.target / 'data' / 'uploads').resolve()
            escaped = path and not str((uploads / path[0]).resolve()).startswith(str(uploads) + chr(92)) and not str((uploads / path[0]).resolve()).startswith(str(uploads) + '/')
            if escaped:
                finding('ATT-2', 'critical', '文件名路径穿越', 'filename=%r 落盘 path=%r' % (fname, path[0]))
            # 清理
            c.request('DELETE', '/admin/attachment/delete?xid=' + xid, None, ck)

    # B3: modelName 路径穿越（存储子目录）
    for model in ('../../evil', '../..\\evil', 'a/b/../../../c'):
        ctype, body = multipart_body({'modelName': model}, 'file', 'm.bin', b'y' * 5)
        s, _, b_, _ = c.request('POST', '/admin/attachment/upload', body, ck, ctype)
        if jload(b_) and jload(b_).get('result'):
            xid = jload(b_)['data']['xid']
            with sqlite3.connect(SRV.target / 'db/main.db') as db:
                path = db.execute('SELECT path FROM attachment WHERE xid=?', (xid,)).fetchone()[0]
            norm = str(Path(path).resolve()) if path else ''
            base = str((SRV.target / 'data' / 'uploads').resolve())
            if not norm.startswith(base):
                finding('ATT-3', 'high', 'modelName 目录穿越', 'model=%r → path=%r' % (model, path))
            c.request('DELETE', '/admin/attachment/delete?xid=' + xid, None, ck)

    # B4: xid 参数注入（下载入口的 SQL/路径/超长）
    upload = multipart_body({'modelName': 'probe'}, 'file', 't.zip', b'z' * 32)
    s, _, b_, _ = c.request('POST', '/admin/attachment/upload', upload[1], ck, upload[0])
    real_xid = jload(b_)['data']['xid']
    for evil in ("x' OR '1'='1", '../../../../db/main.db', 'x' * 500, real_xid + '%00',
                 '../' * 20 + 'db/main.db'):
        s, _, b_, _ = c.request('GET', '/attachment?xid=' + evil.replace(' ', '%20'), None, mck)
        if s == 200 and b'database' in b_[:200].lower() or (s == 200 and len(evil) > 100):
            finding('ATT-4', 'critical', 'xid 参数可取到任意文件', 'xid=%r → %d' % (evil[:40], s))
        elif s == 500:
            finding('ATT-5', 'medium', 'xid 异常输入 500', 'xid=%r' % evil[:40])

    # B5: 购买竞态（并发 20 购买同一付费附件 → 余额/订单一致性）
    seller = member_cookie(c, 'att_seller_p')
    ctype, body = multipart_body({'modelName': 'shop', 'accessType': '2', 'price': '5', 'priceType': '0'},
                                 'file', 'paid.zip', b'RACE')
    s, _, b_, _ = c.request('POST', '/api/v1/attachment/upload', body, seller, ctype)
    paid_xid = jload(b_)['data']['xid']
    buyers = []
    for i in range(20):
        buyers.append((i, member_cookie(c, 'race_buyer_%d' % i, deposit=100)))
    def buy(item):
        i, bck = item
        cc = Client(PORT, timeout=20)
        return cc.request('POST', '/api/v1/attachment/purchase', {'xid': paid_xid}, bck)[0]
    with ThreadPoolExecutor(20) as ex:
        codes = list(ex.map(buy, buyers))
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        orders = db.execute('SELECT COUNT(*) FROM attachmentOrder WHERE attachmentXid=?', (paid_xid,)).fetchone()[0]
        neg = db.execute('SELECT COUNT(*) FROM member WHERE balance < 0').fetchone()[0]
        spent = db.execute("SELECT SUM(price) FROM attachmentOrder WHERE attachmentXid=?", (paid_xid,)).fetchone()[0] or 0
    if neg > 0:
        finding('ATT-6', 'critical', '购买竞态导致负余额', 'balance<0 的会员数=%d' % neg)
    if orders > 20:
        finding('ATT-7', 'high', '购买竞态重复订单', 'orders=%d > 20' % orders)
    # 校验买家总扣款与订单一致
    total_ok = True
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        for i in range(20):
            bal = db.execute("SELECT balance FROM member WHERE username=?", ('race_buyer_%d' % i,)).fetchone()
            if bal and bal[0] < 95:  # 最多一次成功扣 5
                total_ok = False
    if not total_ok:
        finding('ATT-8', 'high', '购买竞态多扣款', '存在扣款超过一次的买家')

    # B6: 负价格 / 篡改保存（admin save 负价 → 购买反向加钱？）
    s, _, b_, _ = c.request('POST', '/admin/attachment/save',
                            {'xid': paid_xid, 'allowHotlink': False, 'accessType': 2,
                             'accessLevel': 0, 'price': -100, 'priceType': 0, 'remark': ''}, ck)
    if jload(b_) and jload(b_).get('result'):
        bck = member_cookie(c, 'neg_buyer', deposit=0)
        s, _, b_, _ = c.request('POST', '/api/v1/attachment/purchase', {'xid': paid_xid}, bck)
        with sqlite3.connect(SRV.target / 'db/main.db') as db:
            bal = db.execute("SELECT balance FROM member WHERE username='neg_buyer'").fetchone()[0]
        if bal > 0:
            finding('ATT-9', 'critical', '负价格购买反向加钱', 'buyer balance=%d' % bal)

    # B7: 防盗链白名单绕过（伪造 Referer 变体；先对 real_xid 关闭 allowHotlink）
    s, _, b_, _ = c.request('POST', '/admin/attachment/save',
                            {'xid': real_xid, 'allowHotlink': False, 'accessType': 0,
                             'accessLevel': 0, 'price': 0, 'priceType': 0, 'remark': ''}, ck)
    for ref in ('http://127.0.0.1.evil.com/', 'http://evil.com/127.0.0.1/', 'file:///127.0.0.1/x'):
        s, _, b_, _ = c.request('GET', '/attachment?xid=' + real_xid, None, mck, raw_headers={'Referer': ref})
        if s == 200:
            finding('ATT-10', 'medium', '防盗链白名单可伪造', 'Referer=%r 放行' % ref)

    # B8: multipart 模糊（畸形不崩为准）
    fuzz_cases = [
        ('无边界头', b'x' * 100, 'application/json'),
        ('边界超长', multipart_body({}, 'file', 'f.bin', b'x', boundary='A' * 300)[1], 'multipart/form-data; boundary=' + 'A' * 300),
        ('非闭合', b'--B\r\nContent-Disposition: form-data; name="file"; filename="x"\r\n\r\nDATA', 'multipart/form-data; boundary=B'),
        ('裸 LF', b'--B\nContent-Disposition: form-data; name="file"; filename="x"\n\nDATA\n--B--', 'multipart/form-data; boundary=B'),
        ('空体', b'', 'multipart/form-data; boundary=B'),
        ('二进制垃圾', bytes(range(256)) * 20, 'multipart/form-data; boundary=B'),
								('千部件', b''.join(
												b'--B' + bytes([13, 10]) + b'Content-Disposition: form-data; name=' + str(i).encode() + b'' + bytes([13, 10, 13, 10]) + b'v' + bytes([13, 10])
												for i in range(500)) + b'--B--' + bytes([13, 10]), 'multipart/form-data; boundary=B'),
    ]
    for label, body, ctype in fuzz_cases:
        s, _, b_, dt = c.request('POST', '/admin/attachment/upload', body, ck, ctype, timeout=30)
        if s == 0 or not SRV.alive():
            finding('ATT-11', 'critical', 'multipart 模糊致连接中断/崩溃', 'case=%s' % label)
            SRV.boot()
            globals()['SRV'] = SRV
        elif dt > 5:
            finding('ATT-12', 'medium', 'multipart 模糊耗时异常', 'case=%s %.1fs' % (label, dt))

    # B9: 会员越权调后台接口
    upload_pair = multipart_body({}, 'file', 'x.zip', b'v')
    s, _, b_, _ = c.request('POST', '/admin/attachment/upload', upload_pair[1], mck, upload_pair[0])
    if s == 200 and jload(b_) and jload(b_).get('result'):
        finding('ATT-13', 'high', '会员可调后台附件上传', 'member cookie 通过 /admin/attachment/upload')
    s, _, b_, _ = c.request('GET', '/admin/attachment/list?page=1&limit=10', None, mck)
    if s == 200 and jload(b_):
        finding('ATT-14', 'high', '会员可读后台附件列表', '')
    s, _, b_, _ = c.request('DELETE', '/admin/attachment/delete?xid=' + real_xid, None, ck)
    s, _, b_, _ = c.request('DELETE', '/admin/attachment/delete?xid=' + paid_xid, None, ck)
    finding('B-att-done', 'info', 'attachment 渗透阶段完成', '')


def phase_b_sched(c, ck, mck):
    STATE['phase'] = 'B-sched'
    save_state()

    # S1: 会员越权（RCE 面）
    s, _, b_, _ = c.request('GET', '/admin/sched/tasks?page=1&limit=5', None, mck)
    if s == 200 and jload(b_):
        finding('SCH-1', 'high', '会员可读计划任务列表', '')
    s, _, b_, _ = c.request('POST', '/admin/sched/task/run?id=1', None, mck)
    if s == 200 and jload(b_) and jload(b_).get('result'):
        finding('SCH-2', 'critical', '会员可触发任务执行', 'RCE 面')
    s, _, b_, _ = c.request('POST', '/admin/sched/task/save', {
        'name': 'evil', 'execType': 'shell', 'scheduleType': 'once',
        'onceAt': 1, 'codeText': '@echo hacked\n'}, mck)
    if s == 200 and jload(b_) and jload(b_).get('result'):
        finding('SCH-3', 'critical', '会员可创建任务（RCE）', '')

    # S2: cron 表达式模糊
    evil_crons = [
        '*/0 * * * * *', '99 * * * * *', '* * * * * * * *', '-1 * * * * *',
        '* 0-100 * * * *', 'a/b * * * * *', '*/99999999999999999999 * * * * *',
        '*/2 1-1-1 * * * *', ',,, * * * * *', '* ' * 50,
        '0 0 0 31 2 *', '0 0 0 1 1 * 1970', '0 0 0 1 1 * 9999',
        '*/1 * * * * 2026-2200',
    ]
    for expr in evil_crons:
        s, _, b_, dt = c.request('POST', '/admin/sched/preview', {
            'scheduleType': 'cron', 'cronExpr': expr, 'execType': 'shell',
            'codeText': 'x', 'name': 'fz'}, ck, timeout=30)
        if s == 0 or not SRV.alive():
            finding('SCH-4', 'critical', 'cron 模糊崩溃', 'expr=%r' % expr[:50])
            SRV.boot()
        elif dt > 3:
            finding('SCH-5', 'high', 'cron 模糊耗时异常 %.1fs' % dt, 'expr=%r' % expr[:50])
        elif s == 200 and jload(b_) and jload(b_).get('result') and '*/0' in expr:
            finding('SCH-6', 'medium', '非法步长 0 被接受', expr)

    # S3: 字段滥用（超长名、负值）
    s, _, b_, dt = c.request('POST', '/admin/sched/task/save', {
        'name': 'N' * 100000, 'execType': 'shell', 'scheduleType': 'once',
        'onceAt': 1, 'codeText': 'x'}, ck, timeout=30)
    if dt > 3:
        finding('SCH-7', 'medium', '超长任务名耗时 %.1fs' % dt, '')
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        db.execute("DELETE FROM sched_task WHERE name LIKE 'N%'")
        db.commit()
    for field, value in (('intervalValue', -5), ('timeoutSec', -1), ('onceAt', -1),
                         ('parallelLimit', -1), ('retryDelaySec', -1)):
        s, _, b_, _ = c.request('POST', '/admin/sched/task/save', {
            'name': 'negtest', 'execType': 'shell', 'scheduleType': 'interval',
            'intervalValue': 60, 'intervalUnit': 'second', 'timeoutSec': 10,
            'codeText': '@echo n\n', field: value}, ck)
        r = jload(b_)
        if r and r.get('result'):
            finding('SCH-8', 'medium', '负值字段被接受: ' + field, str(value))
            with sqlite3.connect(SRV.target / 'db/main.db') as db:
                db.execute("DELETE FROM sched_task WHERE name='negtest'")
                db.commit()

    # S4: 导入类型混淆
    for payload in ({'a': 1}, [None] * 100,
                    [{'name': ['not', 'string'], 'execType': 'shell',
                      'scheduleType': 'once', 'onceAt': 1, 'codeText': 'x'}],
                    [{'name': 'x' * 100000, 'execType': 5, 'scheduleType': {}, 'onceAt': 'abc'}]):
        s, _, b_, dt = c.request('POST', '/admin/sched/import', payload, ck, timeout=30)
        if s == 0 or not SRV.alive():
            finding('SCH-9', 'critical', '导入模糊崩溃', repr(payload)[:80])
            SRV.boot()
        elif dt > 3:
            finding('SCH-10', 'medium', '导入模糊耗时 %.1fs' % dt, repr(payload)[:60])

    # S5: workDir 穿越
    s, _, b_, _ = c.request('POST', '/admin/sched/task/save', {
        'name': 'wdtest', 'execType': 'shell', 'shellType': 'cmd', 'scheduleType': 'once',
        'onceAt': int(time.time() * 1000000) + 3600 * 1000000, 'timeoutSec': 10,
        'workDir': '..' + chr(92) * 0 + '/../../Windows', 'codeText': '@echo wd\n'}, ck)
    r = jload(b_)
    if r and r.get('result'):
        s, _, b_, _ = c.request('POST', '/admin/sched/task/run?id=%d' % r.get('id', 0), None, ck)
        time.sleep(2)
        with sqlite3.connect(SRV.target / 'db/main.db') as db:
            row = db.execute("SELECT lastStatus, lastMessage FROM sched_task WHERE name='wdtest'").fetchone()
        if row and row[0] == 'success':
            finding('SCH-11', 'high', 'workDir 可指系统目录执行', str(row[1]))
        with sqlite3.connect(SRV.target / 'db/main.db') as db:
            db.execute("DELETE FROM sched_task WHERE name='wdtest'")
            db.commit()

    # S6: C 任务代码注入逃逸（破坏 runner 结构，宿主不受影响为准）
    escape_code = 'int TaskProc(TaskInfo* info) {\n    return 0;\n}\n\n#include <windows.h>\n'
    s, _, b_, _ = c.request('POST', '/admin/sched/task/save', {
        'name': 'injtest', 'execType': 'c', 'scheduleType': 'once',
        'onceAt': int(time.time() * 1000000) + 3600 * 1000000, 'timeoutSec': 20,
        'codeText': escape_code}, ck)
    if jload(b_) and jload(b_).get('result'):
        tid = jload(b_)['id']
        s, _, b_, _ = c.request('POST', '/admin/sched/task/run?id=%d' % tid, None, ck)
        time.sleep(4)
        if not SRV.alive():
            finding('SCH-12', 'critical', 'C 任务代码注入致宿主崩溃', '')
            SRV.boot()
        with sqlite3.connect(SRV.target / 'db/main.db') as db:
            db.execute("DELETE FROM sched_task WHERE name='injtest'")
            db.commit()
    finding('B-sched-done', 'info', 'sched 渗透阶段完成', '')


def phase_b_notify(c, ck, mck):
    STATE['phase'] = 'B-notify'
    save_state()

    # N1: 会员越权
    s, _, b_, _ = c.request('POST', '/admin/member/notify', {
        'sendType': 'all', 'title': 'evil', 'content': 'evil'}, mck)
    if s == 200 and jload(b_) and jload(b_).get('result'):
        finding('NOT-1', 'critical', '会员可群发站内信', '')
    s, _, b_, _ = c.request('GET', '/admin/member/notify?page=1&limit=5', None, mck)
    if s == 200 and jload(b_):
        finding('NOT-2', 'high', '会员可读站内信管理列表', '')

    # N2: 越权访问他人消息
    victim = member_cookie(c, 'notify_victim')
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        vid = db.execute("SELECT id FROM member WHERE username='notify_victim'").fetchone()[0]
    c.request('POST', '/admin/member/notify', {
        'sendType': 'users', 'memberIds': str(vid), 'title': 'for-victim', 'content': 'secret'}, ck)
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        rid = db.execute("SELECT id FROM notify_recipient WHERE memberId=? ORDER BY id DESC LIMIT 1", (vid,)).fetchone()[0]
    s, _, b_, _ = c.request('GET', '/api/v1/notify/detail?id=%d' % rid, None, mck)
    r = jload(b_)
    if s == 200 and r and r.get('data') and r['data'].get('content') == 'secret':
        finding('NOT-3', 'high', '会员可读他人站内信', 'recipient 隔离失效')
    s, _, b_, _ = c.request('POST', '/api/v1/notify/delete', {'id': rid}, mck)
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        gone = db.execute("SELECT isDelete FROM notify_recipient WHERE id=?", (rid,)).fetchone()
    if gone and gone[0] == 1:
        finding('NOT-4', 'high', '会员可删他人站内信', '')

    # N3: 大参数
    s, _, b_, dt = c.request('POST', '/admin/member/notify', {
        'sendType': 'users', 'memberIds': ','.join(str(i) for i in range(1, 20000)),
        'title': 'big', 'content': 'x'}, ck, timeout=60)
    if dt > 10:
        finding('NOT-5', 'medium', '2 万 ID 发送耗时 %.1fs' % dt, '收件人去重为 O(n²) 扫描')
    s, _, b_, dt = c.request('POST', '/api/v1/notify/read', {
        'ids': list(range(1, 20000))}, mck, timeout=60)
    if dt > 10:
        finding('NOT-6', 'medium', '2 万 ID 已读耗时 %.1fs' % dt, '')
    s, _, b_, dt = c.request('POST', '/admin/member/notify', {
        'sendType': 'all', 'title': 'T' * 1000000, 'content': 'C' * 1000000}, ck, timeout=60)
    if dt > 10:
        finding('NOT-7', 'low', '1MB 标题发送耗时 %.1fs' % dt, '')

    # N4: 存储型 XSS 载荷入库（前端渲染待验证）
    s, _, b_, _ = c.request('POST', '/admin/member/notify', {
        'sendType': 'users', 'memberIds': '1', 'title': '<img src=x onerror=alert(1)>',
        'content': '<script>alert(1)</script>', 'actionUrl': 'javascript:alert(1)'}, ck)
    if jload(b_) and jload(b_).get('result'):
        finding('NOT-8', 'low', 'XSS 载荷可入库', '前端 layui 渲染方式待验证（需人裁定）')
    finding('B-notify-done', 'info', 'notify 渗透阶段完成', '')


# ==================== 阶段 C：经典渗透复刻 ====================

def phase_c_classic(c, ck, mck):
    STATE['phase'] = 'C-classic'
    save_state()
    # C1: 未授权访问所有管理接口（无 cookie 直取）
    admin_paths = [
        '/admin/menu', '/admin/auth/user?page=1&limit=10', '/admin/member/user?page=1&limit=10',
        '/admin/logs?page=1&limit=10', '/admin/option/get?file=global.json',
        '/admin/sched/tasks?page=1&limit=10', '/admin/attachment/list?page=1&limit=10',
        '/admin/member/notify?page=1&limit=10', '/admin/plugin/list', '/admin/trace/route',
        '/api/v1/profile', '/api/v1/notify/list', '/api/v1/attachment/my',
    ]
    for p in admin_paths:
        s, _, b_, _ = c.request('GET', p)
        if s == 200:
            finding('CL-1', 'high', '未授权访问: ' + p, '无会话返回 200')

    # C2: SQL 注入（全部新家族的查询参数）
    sqli = ("' OR '1'='1' --", "1; DROP TABLE member--", "1 UNION SELECT 1,2,3--",
            "%", "_'%00")
    for p_base in ('/admin/sched/tasks?page=1&limit=10&search=',
                   '/admin/auth/user?page=1&limit=10&search=',
                   '/admin/member/user?page=1&limit=10&search=',
                   '/admin/logs?page=1&limit=10&search=',
                   '/attachment?xid='):
        for payload in sqli:
            s, _, b_, _ = c.request('GET', p_base + payload.replace(' ', '%20').replace("'", '%27'), None, ck)
            if s == 500:
                finding('CL-2', 'medium', 'SQL 注入引发 500: ' + p_base, payload[:40])
            elif s == 200 and (b'sqlite3' in b_.lower() or b'unrecognized token' in b_):
                finding('CL-3', 'critical', 'SQL 注入回显: ' + p_base, payload[:40])

    # C3: 会话固定/伪造
    s, _, b_, _ = c.request('GET', '/api/v1/profile', None, 'MSID=' + 'A' * 32)
    if s == 200:
        finding('CL-4', 'high', '伪造 MSID 可通过', '任意 32 字符被接受')
    s, _, b_, _ = c.request('GET', '/admin/menu', None, 'XSID=' + '0' * 32)
    if s == 200:
        finding('CL-5', 'high', '伪造 XSID 可通过', '')

    # C4: 权限升级（普通管理员改超管口令/角色）
    # smoke 库只有 migration_smoke 一个管理员——改用角色越权（member authLevel 提权尝试）
    s, _, b_, _ = c.request('POST', '/api/v1/profile', {'authLevel': 999, 'status': 1}, mck)
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        lvl = db.execute("SELECT authLevel FROM member WHERE username=?", ('campaign_m',)).fetchone()
    # N/A：profile 只允许特定字段——由 CL-9 补充

    # C5: 方法混淆（GET 打写接口）
    for p in ('/admin/attachment/upload', '/admin/sched/task/save'):
        s, _, b_, _ = c.request('GET', p, None, ck)
        if s == 200 and jload(b_) and jload(b_).get('result'):
            finding('CL-6', 'medium', 'GET 可触发写接口: ' + p, '')

    # C6: 畸形 HTTP（原始报文）
    raws = [
        b'GET /admin/login HTTP/0.9\r\n\r\n',
        b'GETT / HTTP/1.1\r\nHost: x\r\n\r\n',
        b'GET ' + b'/' * 20000 + b' HTTP/1.1\r\nHost: x\r\n\r\n',
        b'POST /api/v1/login HTTP/1.1\r\nHost: x\r\nContent-Length: -1\r\n\r\n',
        b'POST /api/v1/login HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\nZZZZ',
        b'GET / HTTP/1.1\r\nHost: x\r\nCookie: ' + b'A' * 60000 + b'\r\n\r\n',
    ]
    for raw in raws:
        try:
            out = c.raw(raw)
            if b'200 OK' in out and b'20000' in str(len(raw)):
                pass
        except (OSError, socket.timeout):
            pass
    if not SRV.alive():
        finding('CL-7', 'critical', '畸形报文致崩溃', '')
        SRV.boot()

    # C7: 暴力破解限速确认（20 次错密 → 冷却）
    for i in range(20):
        c.request('POST', '/admin/login', {'username': ADMIN, 'password': 'wrong' + str(i)})
    s, _, b_, dt = c.request('POST', '/admin/login', {'username': ADMIN, 'password': 'again'})
    if jload(b_) and jload(b_).get('result') is True:
        finding('CL-8', 'high', '暴力破解防护未生效', '20 次失败后仍可尝试')
    finding('C-classic-done', 'info', '经典渗透阶段完成', '')


# ==================== 阶段 D：持续混合浸泡 ====================

def phase_d_soak(c, ck, mck, hours):
    STATE['phase'] = 'D-soak'
    save_state()
    deadline = time.time() + hours * 3600
    rss0, h0 = process_stats(SRV.pid)
    counter = {'req': 0, 'err': 0}
    lock = threading.Lock()
    stop = threading.Event()

    # 准备写负载资产
    upload_pair = multipart_body({'modelName': 'soak'}, 'file', 's.zip', b'S' * 2048)
    s, _, b_, _ = c.request('POST', '/admin/attachment/upload', upload_pair[1], ck, upload_pair[0])
    soak_xid = jload(b_)['data']['xid'] if jload(b_) and jload(b_).get('result') else None

    def hammer(idx, profile):
        cc = Client(PORT, timeout=30)
        my = {'req': 0, 'err': 0}
        while not stop.is_set() and time.time() < deadline:
            try:
                if profile == 0:   # 只读管理面
                    s, _, _, _ = cc.request('GET', '/admin/auth/user?page=1&limit=10', None, ck)
                elif profile == 1:  # 会员 API
                    s, _, _, _ = cc.request('GET', '/api/v1/notify/unread_count', None, mck)
                elif profile == 2:  # 附件下载
                    s, _, _, _ = cc.request('GET', '/attachment?xid=' + (soak_xid or 'x'), None, mck)
                elif profile == 3:  # 通知写
                    s, _, _, _ = cc.request('POST', '/admin/member/notify',
                                            {'sendType': 'users', 'memberIds': '1',
                                             'title': 'soak', 'content': 'x'}, ck)
                elif profile == 4:  # sched 面板
                    s, _, _, _ = cc.request('GET', '/admin/sched/dashboard', None, ck)
                elif profile == 5:  # 登录流
                    s, _, _, _ = cc.request('POST', '/api/v1/login',
                                            {'username': 'soak_m', 'password': 'x'})
                else:               # trace/option
                    s, _, _, _ = cc.request('GET', '/admin/trace/route', None, ck)
                my['req'] += 1
                if s >= 500 or s == 0:
                    my['err'] += 1
            except OSError:
                my['err'] += 1
                time.sleep(0.2)
        with lock:
            counter['req'] += my['req']
            counter['err'] += my['err']

    threads = [threading.Thread(target=hammer, args=(i, i % 7), daemon=True) for i in range(14)]
    for t in threads:
        t.start()

    sample_n = 0
    reloads = 0
    last_rss, last_h = rss0, h0
    while time.time() < deadline and SRV.alive():
        time.sleep(300)
        sample_n += 1
        rss, h = process_stats(SRV.pid)
        lat_probe = Client(PORT, timeout=20)
        _, _, _, lat = lat_probe.request('GET', '/admin/auth/user?page=1&limit=10', None, ck)
        metrics_row('D', 'soak-sample', 14, [lat], 0,
                    {'rss_mb': rss, 'handles': h, 'total_req': counter['req'],
                     'total_err': counter['err'], 'minutes': round((time.time() - STATE['started']) / 60, 0)})
        # 资源漂移告警
        if rss - last_rss > 100:
            finding('D-rss', 'medium', 'RSS 5 分钟上涨 %dMB' % (rss - last_rss),
                    'rss=%dMB（起点 %dMB）' % (rss, rss0))
        last_rss, last_h = rss, h
        save_state()
        # 每小时触发一次主脚本热重载（换代安全）
        if sample_n % 2 == 0 and SRV.alive():
            try:
                rr = Client(PORT, timeout=20)
                rr.request('POST', '/__test/expire', None, ck)
                s, _, b_, _ = Client(PORT, timeout=20).request('GET', '/admin/plugin/list', None, ck)
                reloads += 1
            except OSError:
                pass
        if not SRV.alive():
            finding('D-crash', 'critical', '浸泡期宿主崩溃', '存活 %d 分钟' % ((time.time() - STATE['started']) / 60))
            break
    stop.set()
    for t in threads:
        t.join(timeout=10)
    rss1, h1 = process_stats(SRV.pid) if SRV.alive() else (0, 0)
    metrics_row('D', 'soak-final', 14, [], 0,
                {'rss_mb': rss1, 'handles': h1, 'total_req': counter['req'],
                 'total_err': counter['err'], 'reloads': reloads})
    finding('D-done', 'info', '浸泡完成',
            'req=%d err=%d rss %d→%dMB hdl %d→%d reloads=%d' %
            (counter['req'], counter['err'], rss0, rss1, h0, h1, reloads))


# ==================== 主流程 ====================

SRV = Server()

def main():
    RUNTIME.mkdir(parents=True, exist_ok=True)
    for f in ('campaign_metrics.jsonl', 'campaign_findings.json', 'campaign_state.json'):
        (RUNTIME / f).write_text('', encoding='utf-8')
    print('[campaign] booting fixture...', flush=True)
    SRV.boot()
    c = Client(PORT, timeout=30)
    # 降噪：根库自带的示例任务以 10s 节奏全量拉起 xs.exe runner，压测期关闭
    # （生产库该任务是否保留由运营裁定——见报告待裁定项）
    with sqlite3.connect(SRV.target / 'db/main.db') as db:
        db.execute('UPDATE sched_task SET enabled = 0')
        db.commit()
    ck = admin_cookie(c)
    mck = member_cookie(c, 'campaign_m', deposit=1000)
    root_db_hash = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest()

    t_total = time.time() + DURATION_HOURS * 3600
    def guarded(name, fn):
        try:
            fn()
        except Exception:
            finding('HARNESS', 'medium', '阶段 %s 异常（继续后续阶段）' % name, traceback.format_exc()[-800:])
            if not SRV.alive():
                SRV.boot()
    try:
        guarded('A', lambda: phase_a_sweep(c, ck, mck))
        guarded('B-att', lambda: phase_b_attachment(c, ck, mck))
        guarded('B-sched', lambda: phase_b_sched(c, ck, mck))
        guarded('B-notify', lambda: phase_b_notify(c, ck, mck))
        guarded('C', lambda: phase_c_classic(c, ck, mck))
        # D：剩余时间全部浸泡
        remain_h = max(0.2, (t_total - time.time()) / 3600)
        phase_d_soak(c, ck, mck, remain_h)
    except Exception:
        finding('FATAL', 'critical', '战役异常终止', traceback.format_exc()[-1500:])
    finally:
        SRV.stop()
        if hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest() != root_db_hash:
            finding('INTEG', 'critical', '根库被污染', '')
        STATE['phase'] = 'done'
        save_state()
        print('[campaign] finished', flush=True)


if __name__ == '__main__':
    main()
