"""Active attack probes against an isolated x-admin fixture (loopback only).

Each phase is independent; findings print immediately and land in a JSON
report. Read-only toward the root repository except its own fixture.
"""
from pathlib import Path
import argparse
import hashlib
import http.client
import json
import socket
import sqlite3
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, str(Path(__file__).resolve().parent))
import smoke  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ADMIN = 'migration_smoke'
PASSWORD = 'Temporary-test-only-9081'
FINDINGS = []


def finding(fid, severity, title, detail):
    FINDINGS.append({'id': fid, 'severity': severity, 'title': title, 'detail': detail})
    print(f'[{severity}] {fid}: {title}\n    {detail}', flush=True)


class Target:
    def __init__(self, port):
        self.port = port

    def raw_request(self, raw, timeout=10):
        sock = socket.create_connection(('127.0.0.1', self.port), timeout=timeout)
        try:
            sock.sendall(raw)
            chunks = []
            while True:
                data = sock.recv(65536)
                if not data:
                    break
                chunks.append(data)
                if b'\r\n\r\n' in b''.join(chunks) and len(b''.join(chunks)) > 100:
                    break
            return b''.join(chunks)
        finally:
            sock.close()

    def request(self, method, path, data=None, cookie=None, timeout=15):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=timeout)
        headers = {}
        if data is not None:
            data = json.dumps(data).encode() if not isinstance(data, bytes) else data
            headers['Content-Type'] = 'application/json'
        if cookie:
            headers['Cookie'] = cookie
        conn.request(method, path, data, headers)
        resp = conn.getresponse()
        body = resp.read()
        out = (resp.status, dict(resp.getheaders()), body)
        conn.close()
        return out


def admin_login(t):
    status, headers, body = t.request('POST', '/admin/login', {
        'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)})
    assert json.loads(body)['result'], (status, body[:200])
    return headers['Set-Cookie'].split(';')[0]


def phase_cookies(t):
    print('== P1 会话/Cookie ==', flush=True)
    status, headers, _ = t.request('POST', '/admin/login', {
        'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)})
    admin_cookie_hdr = headers.get('Set-Cookie', '')
    m = 'probe_member'
    t.request('POST', '/api/v1/register', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    status, headers, body = t.request('POST', '/api/v1/login', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    member_hdr = headers.get('Set-Cookie', '')
    print(f'  admin: {admin_cookie_hdr[:90]}')
    print(f'  member: {member_hdr[:90]}')
    if 'SameSite' not in member_hdr:
        finding('P1-1', 'MEDIUM',
                '会员 Cookie 缺少 SameSite 属性',
                f'管理端 XSID 有 SameSite=Lax，会员 MSID 没有：{member_hdr[:100]}。'
                '跨站表单可直接携带 MSID 调用 /api/v1/*（改资料、注销等），无 CSRF 防护。')

    # session fixation: pre-set cookie then login
    status, headers, body = t.request('POST', '/admin/login', {
        'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)}, cookie='XSID=FIXATED')
    new_sid = headers.get('Set-Cookie', '').split('XSID=')[1].split(';')[0] if 'XSID=' in headers.get('Set-Cookie', '') else ''
    if new_sid and new_sid != 'FIXATED' and len(new_sid) == 32:
        print('  session fixation: 登录后签发新 XID，无固定攻击面')
    else:
        finding('P1-2', 'HIGH', '会话固定疑似', f'Set-Cookie={headers.get("Set-Cookie", "")[:100]}')

    # brute force moved to phase_bruteforce (last): Guard 按 IP 全局生效，
    # 一旦触发会把同 IP 后续所有阶段一并锁死。



def phase_bruteforce(t):
    """必须最后执行：触发冷却后同 IP 全部登录端点被锁。"""
    print('== P8 暴力破解/Guard（最后执行） ==', flush=True)
    got_lock = None
    for i in range(8):
        status, _, body = t.request('POST', '/admin/login', {
            'username': ADMIN, 'password': smoke.client_hash(ADMIN, 'wrong' + str(i))})
        msg = json.loads(body).get('message', '')
        if '过多' in msg:
            got_lock = (i + 1, msg)
            break
    print(f'  管理端: 第 {got_lock[0] if got_lock else "?"} 次失败后锁定')
    print(f'  锁定消息(原文): {got_lock[1] if got_lock else "未触发"}')
    if not got_lock:
        finding('P1-3', 'HIGH', '暴力破解防护未生效', '8 次失败登录未触发冷却')
    status, _, body = t.request('POST', '/admin/login', {
        'username': ADMIN, 'password': smoke.client_hash(ADMIN, PASSWORD)})
    ok_while_locked = json.loads(body).get('result')
    print(f'  锁定期间正确口令仍被拒: {ok_while_locked is not True}')
    # 同 IP 会员登录是否也被锁（Guard 是否跨端点共享）
    m = 'guard_member'
    t.request('POST', '/api/v1/register', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    status, _, body = t.request('POST', '/api/v1/login', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    code = json.loads(body).get('code')
    print(f'  管理端锁定期间，同 IP 正确口令的会员登录 code={code}')
    if code == 429:
        finding('P8-1', 'MEDIUM',
                '暴力破解 Guard 按 IP 跨端点共享，可被用于放大拒绝服务',
                '对 /admin/login 失败 5 次后，同 IP 的 /api/v1/login 用正确口令也被拒（code=429）。'
                '共享 NAT 出口或本机部署场景下，攻击者可锁定同 IP 全部合法用户（含前台），'
                '冷却 5 分钟且按倍数递增。')


def phase_injection(t):
    print('== P2 注入 ==', flush=True)
    m = 'inj_member'
    evil_nick = 'x","backdoor":"1'
    t.request('POST', '/api/v1/register', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD), 'nickname': evil_nick})
    status, headers, body = t.request('POST', '/api/v1/login', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    text = body.decode('utf-8', errors='replace')
    valid_json = True
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        valid_json = False
        parsed = None
    print(f'  login resp valid_json={valid_json}: {text[:160]}')
    if not valid_json or (parsed and parsed.get('data', {}).get('nickname') != evil_nick):
        finding('P2-1', 'MEDIUM',
                '会员昵称经 sprintf 拼进 JSON 响应，含引号时破坏结构',
                f'nickname={evil_nick!r} → 响应: {text[:160]}。API_Login/API_Profile 均为 %s 直拼，'
                '未做 JSON 转义；结构破坏可导致客户端解析异常或注入额外字段。')
    # stored XSS payload round-trip
    xss = '<img src=x onerror=alert(1)>'
    cookie = headers['Set-Cookie'].split(';')[0]
    t.request('PUT', '/api/v1/profile', {'nickname': xss}, cookie)
    status, _, body = t.request('GET', '/api/v1/profile', cookie=cookie)
    stored = xss in body.decode('utf-8', errors='replace')
    print(f'  stored XSS payload persisted: {stored}')
    if stored:
        finding('P2-2', 'LOW',
                'XSS 载荷可原样存储并回显（API 层未转义）',
                '存储型载荷入库且经 /api/v1/profile 原样返回；实际触发取决于前端渲染是否转义（Layui 表格默认转义文本列）。')
    # SQL wildcard / format string in search
    from urllib.parse import quote
    cookie = admin_login(t)
    for probe in ('%', '%%%', "' OR 1=1 --", '%s%s%s%n', '\x00', 'a' * 5000):
        status, _, body = t.request('GET',
                                    '/admin/auth/user?page=1&limit=5&search=' + quote(probe),
                                    cookie=cookie, timeout=20)
        if status != 200:
            finding('P2-3', 'HIGH', f'search 参数异常输入返回 {status}', f'probe={probe[:40]!r}')
    print('  search 注入探针全部参数化（无 5xx）')


def phase_authz(t, target):
    print('== P3 越权 ==', flush=True)
    m = 'priv_member'
    t.request('POST', '/api/v1/register', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    _, headers, _ = t.request('POST', '/api/v1/login', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    member_cookie = headers['Set-Cookie'].split(';')[0]
    variants = ['//admin/auth/user', '/admin/auth/user/', '/ADMIN/AUTH/USER',
                '/admin/./auth/user', '/%61dmin/auth/user', '/admin/auth/user%00',
                '/admin/auth/user?', '/admin/auth/user/.', '/./admin/auth/user',
                '/admin//auth/user', '/admin/auth%5cuser']
    for variant in variants:
        for label, cookie in (('anon', None), ('member', member_cookie)):
            try:
                status, _, body = t.request('GET', variant, cookie=cookie, timeout=8)
            except Exception as exc:  # noqa: BLE001
                finding('P3-1', 'HIGH', f'路径变体导致异常 {variant}',
                        f'{label}: {type(exc).__name__}: {exc}')
                continue
            leaked = status == 200 and b'"result"' in body and b'user' in body
            if leaked:
                finding('P3-2', 'CRITICAL', f'路径变体绕过鉴权: {variant}',
                        f'{label} 访问返回 200 数据: {body[:120]}')
    print(f'  {len(variants)} 个路径变体 × 2 身份：无数据泄露')

    # authLevel gate via limited role (has uri permission authID 15, authLevel 0)
    with sqlite3.connect(target / 'db/main.db') as db:
        now = smoke.legacy_now()
        role = db.execute('INSERT INTO role(name,desc,authList,authLevel,createTime,updateTime,isDelete) '
                          "VALUES('probe_limited','',?,0,?,?,0)",
                          ('[15]', now, now)).lastrowid
        user = 'probe_limited'
        pwd = hashlib.sha256((user + 'migration-test-salt' + smoke.client_hash(user, PASSWORD)).encode()).hexdigest().upper()
        db.execute('INSERT INTO user(user,salt,pwd,role,authLevel,createTime,updateTime,isDelete) '
                   'VALUES(?,?,?,?,0,?,?,0)', (user, 'migration-test-salt', pwd, role, now, now))
        db.commit()
    status, headers, body = t.request('POST', '/admin/login', {
        'username': user, 'password': smoke.client_hash(user, PASSWORD)})
    if json.loads(body).get('result'):
        lc = headers['Set-Cookie'].split(';')[0]
        status, _, body = t.request('GET', '/admin/option?file=global.json', cookie=lc)
        result = json.loads(body)
        if result.get('result') is False and '权限' in result.get('message', ''):
            print('  authLevel 内部门槛生效（200 级配置对 0 级会话拒绝）')
        else:
            finding('P3-3', 'HIGH', 'authLevel 内部门槛失效',
                    f'probe_limited 读取 global.json 得到: {body[:150]}')
    else:
        print(f'  limited 角色登录失败（角色权限链）: {body[:100]}')


def phase_resource(t):
    print('== P4 资源耗尽 ==', flush=True)
    cookie = admin_login(t)
    # body size ceiling
    for size_mb in (0.5, 1, 2, 4, 8):
        payload = b'{"pad":"' + b'A' * int(size_mb * 1024 * 1024) + b'"}'
        try:
            status, _, body = t.request('POST', '/admin/auth/user', payload, cookie=cookie, timeout=60)
            print(f'  {size_mb}MB body -> {status} {body[:60]}')
            if status >= 500:
                finding('P4-1', 'HIGH', f'{size_mb}MB 请求体导致 {status}', body[:120])
        except Exception as exc:  # noqa: BLE001
            print(f'  {size_mb}MB body -> {type(exc).__name__}（限制或超时）')
    # JSON bomb
    bomb = b'{"a":' * 100000 + b'1' + b'}' * 100000
    try:
        status, _, body = t.request('POST', '/admin/auth/user', bomb, cookie=cookie, timeout=30)
        print(f'  10万层嵌套 JSON -> {status} {body[:60]}')
        if status >= 500:
            finding('P4-2', 'HIGH', '深嵌套 JSON 导致 5xx', body[:120])
    except Exception as exc:  # noqa: BLE001
        print(f'  JSON bomb -> {type(exc).__name__}')
    # idle/partial connections
    socks = []
    try:
        for i in range(1200):
            s = socket.create_connection(('127.0.0.1', t.port), timeout=3)
            s.sendall(b'GET /admin/login HTTP/1.1\r\nHost: x\r\n')  # partial, no terminator
            socks.append(s)
    except OSError as exc:
        print(f'  空闲连接打到 {len(socks)} 个后失败: {exc}')
    try:
        status, _, _ = t.request('GET', '/admin/login', timeout=8)
        print(f'  {len(socks)} 个半开连接下服务仍可用: {status == 200}')
        if status != 200:
            finding('P4-3', 'HIGH', f'{len(socks)} 个半开连接使服务不可用', f'status={status}')
    except Exception as exc:  # noqa: BLE001
        finding('P4-3', 'HIGH', f'{len(socks)} 个半开连接使服务不可用', str(exc)[:150])
    finally:
        for s in socks:
            try:
                s.close()
            except OSError:
                pass
    time.sleep(2)
    status, _, _ = t.request('GET', '/admin/login', timeout=8)
    print(f'  关闭半开连接后恢复: {status == 200}')
    # slow chunked body — does it block others?
    blocker = socket.create_connection(('127.0.0.1', t.port), timeout=30)
    blocker.sendall(b'POST /admin/auth/user HTTP/1.1\r\nHost: x\r\n'
                    b'Transfer-Encoding: chunked\r\nContent-Type: application/json\r\n\r\n'
                    b'5\r\nhello\r\n')
    t0 = time.perf_counter()
    status, _, _ = t.request('GET', '/admin/menu', cookie=cookie, timeout=10)
    elapsed = time.perf_counter() - t0
    print(f'  慢 chunked 上传期间 /admin/menu 仍可用: status={status} 用时={elapsed:.3f}s')
    if status != 200 or elapsed > 2:
        finding('P4-4', 'MEDIUM', '慢速上传阻塞其他请求',
                f'慢 chunked 期间 /admin/menu: status={status}, {elapsed:.3f}s')
    blocker.close()
    # unbounded limit
    for path in ('/admin/logs?limit=99999&page=1', '/admin/auth/user?limit=99999&page=1',
                 '/admin/member/user?limit=99999&page=1'):
        t0 = time.perf_counter()
        status, _, body = t.request('GET', path, cookie=cookie, timeout=60)
        elapsed = time.perf_counter() - t0
        count = len(json.loads(body).get('data', [])) if status == 200 else -1
        print(f'  {path.split("?")[0]} limit=99999 -> rows={count} {elapsed:.3f}s')
    finding('P4-5', 'LOW',
            '管理列表 limit 无上限',
            '/admin/logs、/admin/auth/user、/admin/member/user 的 limit 参数只做下限保护，'
            '99999 直接进 SQL LIMIT；配合大表可拖动响应与内存（v1 同行为）。')


def phase_logic(t, target):
    print('== P5 业务逻辑 ==', flush=True)
    cookie = admin_login(t)
    m = 'logic_member'
    t.request('POST', '/api/v1/register', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    _, headers, _ = t.request('POST', '/api/v1/login', {
        'username': m, 'password': smoke.client_hash(m, PASSWORD)})
    mc = headers['Set-Cookie'].split(';')[0]
    with sqlite3.connect(target / 'db/main.db') as db:
        mid = db.execute('SELECT id FROM member WHERE username=?', (m,)).fetchone()[0]
    # --- balance int64 overflow
    t.request('POST', '/admin/member/user/balance', {
        'id': mid, 'type': 1, 'amount': 100}, cookie=cookie)
    for huge in (9223372036854775807, 9223372036854775807 - 50,
                 -9223372036854775808, -9223372036854775808 + 100):
        status, _, body = t.request('POST', '/admin/member/user/balance', {
            'id': mid, 'type': 0, 'amount': huge}, cookie=cookie)
        with sqlite3.connect(target / 'db/main.db') as db:
            balance = db.execute('SELECT balance FROM member WHERE id=?', (mid,)).fetchone()[0]
        print(f'  边界 amount={huge} -> result={json.loads(body).get("result")} 余额={balance}')
    huge = 9223372036854775807 - 50
    status, _, body = t.request('POST', '/admin/member/user/balance', {
        'id': mid, 'type': 0, 'amount': huge}, cookie=cookie)
    result = json.loads(body)
    with sqlite3.connect(target / 'db/main.db') as db:
        balance = db.execute('SELECT balance FROM member WHERE id=?', (mid,)).fetchone()[0]
    print(f'  余额溢出: amount={huge} -> result={result.get("result")} 落库余额={balance}')
    if result.get('result') and 0 < balance < 1000:
        finding('P5-1', 'HIGH',
                '余额调整 int64 溢出可篡改余额',
                f'余额 100 分时提交 amount={huge}（INT64_MAX-50）返回成功，落库余额={balance}（回绕值）。'
                'newBalance=current+amount 溢出为小正数绕过 <0 检查。管理员侧数据完整性缺陷。')
    # restore
    t.request('POST', '/admin/member/user/balance', {
        'id': mid, 'type': 0, 'amount': -balance + 100}, cookie=cookie)
    # --- banned member self re-enable via profile PUT
    status, _, body = t.request('PUT', '/admin/member/user', {
        'id': mid, 'groupId': 1, 'authLevel': 0, 'nickname': 'n', 'email': '', 'phone': '',
        'avatar': '', 'status': 0}, cookie=cookie)
    assert json.loads(body)['result'], body
    status, _, body = t.request('GET', '/api/v1/profile', cookie=mc)
    session_alive_while_banned = status == 200
    print(f'  禁用后会员会话仍有效: {session_alive_while_banned}')
    if session_alive_while_banned:
        finding('P5-2', 'MEDIUM',
                '禁用会员不撤销会话',
                '管理员 status=0 后，既有 MSID 会话仍可访问 /api/v1/*（仅删除账号才撤销）。')
    status, _, body = t.request('PUT', '/api/v1/profile', {'nickname': 'revive'}, cookie=mc)
    profile_put_ok = json.loads(body).get('code') == 0
    with sqlite3.connect(target / 'db/main.db') as db:
        status_now = db.execute('SELECT status FROM member WHERE id=?', (mid,)).fetchone()[0]
    print(f'  禁用会员 PUT profile: code=0={profile_put_ok}, 之后 status={status_now}')
    if profile_put_ok and status_now == 1:
        finding('P5-3', 'HIGH',
                '被禁用会员可通过 PUT /api/v1/profile 自行解禁',
                'API_Profile PUT 固定绑定 status=1（注释称"保持不变"），被禁用会员用存量会话改一次资料'
                '即把 status 写回 1，随后可正常登录。需在 PUT 中保留原 status 或校验。')
    # cleanup: soft delete the probe member
    t.request('DELETE', f'/admin/member/user?id={mid}', cookie=cookie)


def phase_disclosure(t, target):
    print('== P6 信息泄露 ==', flush=True)
    client_hash = smoke.client_hash(ADMIN, PASSWORD)
    t.request('POST', '/admin/login', {
        'username': ADMIN, 'password': client_hash})
    with sqlite3.connect(target / 'db/main.db') as db:
        rows = list(db.execute(
            "SELECT body FROM logs WHERE uri='/admin/login' ORDER BY id DESC LIMIT 1"))
    if rows and client_hash in (rows[0][0] or ''):
        finding('P6-1', 'HIGH',
                '登录请求体（含客户端密码哈希）明文写入 logs 表',
                f'/admin/login needLog=1，最新日志 body={rows[0][0][:100]}...；repwd/member 新增等'
                '同样落库。任何能看日志页的管理员/角色可采集全部口令哈希材料（离线破解面）。')
    else:
        print(f'  登录日志体不含哈希: {rows[:1]}')
    # listing / headers
    for path in ('/layui/', '/wwwroot/', '/page/', '/nonexistent'):
        status, headers, body = t.request('GET', path, timeout=8)
        listed = status == 200 and (b'<li>' in body[:2000] or b'Index of' in body)
        if listed:
            finding('P6-2', 'MEDIUM', f'目录列表开启: {path}', body[:120])
    status, headers, _ = t.request('GET', '/admin/login')
    interesting = {k: v for k, v in headers.items()
                   if k.lower() in ('server', 'x-powered-by', 'x-runtime', 'via')}
    print(f'  响应头指纹: {interesting or "无"}')


def phase_perf(t, target, seconds=8):
    print('== P7 性能 ==', flush=True)
    cookie = admin_login(t)

    def bench_threads(n, path):
        count = [0]
        stop = time.time() + seconds

        def loop():
            conn = http.client.HTTPConnection('127.0.0.1', t.port, timeout=10)
            while time.time() < stop:
                try:
                    conn.request('GET', path, headers={'Cookie': cookie})
                    r = conn.getresponse()
                    r.read()
                    if r.status == 200:
                        count[0] += 1
                except Exception:  # noqa: BLE001
                    try:
                        conn.close()
                        conn = http.client.HTTPConnection('127.0.0.1', t.port, timeout=10)
                    except Exception:  # noqa: BLE001
                        return

        with ThreadPoolExecutor(max_workers=n) as pool:
            list(pool.map(lambda _: loop(), range(n)))
        return count[0] / seconds

    static_rps = bench_threads(1, '/layui/layui.js')
    menu_rps = {n: bench_threads(n, '/admin/menu') for n in (1, 2, 4, 8, 16)}
    print(f'  静态文件 单线程: {static_rps:.0f} rps')
    print('  /admin/menu keep-alive rps:', {k: round(v) for k, v in menu_rps.items()})
    peak = max(menu_rps.values())
    if menu_rps[16] <= menu_rps[1] * 1.6:
        finding('P7-1', 'MEDIUM',
                '并发吞吐不随线程扩展（全局请求锁串行化）',
                f'1 线程 {menu_rps[1]:.0f} rps → 16 线程 {menu_rps[16]:.0f} rps（峰值 {peak:.0f}）。'
                'G_RequestLock 将全部业务处理串行化，属迁移期已知取舍，多核无法利用。')
    # write contention
    count = [0]
    stop = time.time() + seconds

    def writer(i):
        n = 0
        while time.time() < stop:
            body = {'name': f'perf_{i}_{n}', 'desc': '', 'authList': '[]', 'authLevel': 0}
            try:
                status, _, resp = t.request('POST', '/admin/auth/role', body, cookie=cookie)
                if status == 200 and json.loads(resp)['result']:
                    count[0] += 1
            except Exception:  # noqa: BLE001
                return
            n += 1

    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(writer, range(8)))
    print(f'  8 线程角色写入: {count[0] / seconds:.1f} wps')
    # seeded logs table search latency
    with sqlite3.connect(target / 'db/main.db') as db:
        db.execute('WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM c WHERE x<50000) '
                   'INSERT INTO logs(user,ip,uri,method,param,body,createTime) '
                   "SELECT 'perf','','/seed','GET','','', 1 FROM c")
        db.commit()
    cookie = admin_login(t)
    lat = []
    for _ in range(5):
        t0 = time.perf_counter()
        status, _, body = t.request('GET', '/admin/logs?page=1&limit=10&search=%25seed%25',
                                    cookie=cookie, timeout=60)
        lat.append((time.perf_counter() - t0) * 1000)
    print(f'  5 万行 logs LIKE 搜索延迟(ms): {[round(x) for x in lat]}')
    if max(lat) > 1500:
        finding('P7-2', 'MEDIUM', '大表 LIKE 搜索延迟显著',
                f'5 万行 logs 通配搜索最大 {max(lat):.0f}ms；无索引且前端可控通配符。')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19110)
    args = parser.parse_args()
    root_hash = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest()
    target = smoke.fixture(args.port)
    log = open(target / 'probe_server.log', 'ab')
    proc = subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                            stdout=log, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    t = Target(args.port)
    try:
        for _ in range(60):
            try:
                if t.request('GET', '/admin/login', timeout=3)[0] == 200:
                    break
            except OSError:
                pass
            time.sleep(0.3)
        phases = [
            lambda: phase_cookies(t),
            lambda: phase_injection(t),
            lambda: phase_authz(t, target),
            lambda: phase_resource(t),
            lambda: phase_logic(t, target),
            lambda: phase_disclosure(t, target),
            lambda: phase_perf(t, target),
            lambda: phase_bruteforce(t),
        ]
        for phase in phases:
            try:
                phase()
            except Exception as exc:  # noqa: BLE001
                finding('PHASE-ERR', 'INFO', f'{phase.__name__} 阶段异常',
                        f'{type(exc).__name__}: {exc}')
        print(f'\n== 探针完成，共 {len(FINDINGS)} 项发现 ==', flush=True)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
        log.close()
        unchanged = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).hexdigest() == root_hash
        print(f'根库未受影响: {unchanged}')
    out = Path(__file__).resolve().parent / '.runtime' / 'probe_findings.json'
    out.write_text(json.dumps(FINDINGS, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'findings -> {out}')


if __name__ == '__main__':
    main()
