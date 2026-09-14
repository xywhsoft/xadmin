# -*- coding: utf-8 -*-
"""NOT-8 / logs 修复专项验证：XSS 输出转义 + actionUrl 白名单 + logs 查询结构。"""
import sys, json, time, sqlite3, subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import smoke
from campaign_10h import Client, admin_cookie, member_cookie, jload, PORT

PORT = 19810

def main():
    target = smoke.fixture(PORT)
    proc = subprocess.Popen(['xs.exe', str(target / 'xs.json')], cwd='.',
                            stdout=open(target / 'dbg.log', 'wb'), stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    c = Client(PORT, timeout=30)
    try:
        for _ in range(60):
            if proc.poll() is not None:
                raise SystemExit('died at boot')
            try:
                if c.request('GET', '/admin/login')[0] == 200: break
            except OSError: pass
            time.sleep(0.25)
        ck = admin_cookie(c)
        mck = member_cookie(c, 'xss_m')

        # ---- 1. XSS 输出转义 ----
        import sqlite3 as _sq
        with _sq.connect(target / 'db/main.db') as _db:
            mid = _db.execute("SELECT id FROM member WHERE username='xss_m'").fetchone()[0]
        payload = {'sendType': 'users', 'memberIds': str(mid),
                   'title': '<img src=x onerror=alert(1)>',
                   'content': '<script>alert(1)</script> & "q" \'i\'',
                   'actionUrl': 'javascript:alert(1)'}
        s, _, b_, _ = c.request('POST', '/admin/member/notify', payload, ck)
        assert jload(b_) and jload(b_)['result'], b_
        s, _, b_, _ = c.request('GET', '/api/v1/notify/list', None, mck)
        rows = (jload(b_) or {}).get('data', [])
        if not rows:
            print('list raw:', s, b_[:200]); raise SystemExit(1)
        row = rows[0]
        assert '<img' not in row['title'] and '&lt;img' in row['title'], row['title']
        assert '<script' not in row['content'] and '&lt;script' in row['content'], row['content']
        assert '&amp;' in row['content'] and '&quot;' in row['content'], row['content']
        assert row['actionUrl'] == '', row['actionUrl']   # javascript: 被白名单拦截
        # 详情同样转义
        s, _, b_, _ = c.request('GET', '/api/v1/notify/detail?id=%d' % row['id'], None, mck)
        d = jload(b_)['data']
        assert '&lt;img' in d['title'] and d['actionUrl'] == '', d
        # 管理列表也转义
        s, _, b_, _ = c.request('GET', '/admin/member/notify?page=1&limit=5', None, ck)
        assert '&lt;img' in json.loads(b_)['data'][0]['title'], b_[:200]
        # 合法 actionUrl 放行
        s, _, b_, _ = c.request('POST', '/admin/member/notify', {
            'sendType': 'users', 'memberIds': str(mid), 'title': 'ok', 'content': 'ok',
            'actionUrl': '/some/page?a=1'}, ck)
        s, _, b_, _ = c.request('GET', '/api/v1/notify/list', None, mck)
        assert json.loads(b_)['data'][0]['actionUrl'] == '/some/page?a=1'
        print('XSS FIX VERIFIED: output escaped, javascript: blocked, relative allowed')

        # ---- 2. logs 查询修复 ----
        with sqlite3.connect(target / 'db/main.db') as db:
            db.execute('CREATE INDEX IF NOT EXISTS idx_logs_createTime ON logs(createTime)')
            n = db.execute('SELECT COUNT(*) FROM logs').fetchone()[0]
        s, _, b_, dt = c.request('GET', '/admin/logs?page=1&limit=10', None, ck)
        r = json.loads(b_)
        assert r['result'] and r['count'] == n and len(r['data']) == min(10, n), (r['count'], n)
        assert 'createTime' in r['data'][0] and 'param' in r['data'][0]
        # 搜索路径
        if n:
            s, _, b_, _ = c.request('GET', '/admin/logs?page=1&limit=10&search=admin', None, ck)
            assert json.loads(b_)['result']
        # 性能探针（本机夹具日志量小，量级验证交给重测）
        t0 = time.time()
        for _ in range(50):
            c.request('GET', '/admin/logs?page=1&limit=10', None, ck)
        print('LOGS FIX VERIFIED: count=%d, 50 req avg %.1fms' % (n, (time.time() - t0) / 50 * 1000))
        print('FIX VERIFY PASS')
    finally:
        proc.terminate()
        try: proc.wait(timeout=10)
        except Exception: proc.kill()

if __name__ == '__main__':
    main()
