# -*- coding: utf-8 -*-
"""attachment 家族功能探针：后台/会员 multipart 上传、列表/详情/保存/删除/统计、
访问控制（登录/付费/级别）、购买事务与防盗链。"""
import sys, json, time, http.client, uuid, subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import smoke

PORT = 19302
HOST = '127.0.0.1'

def multipart(fields, fileField, filename, content):
    b = uuid.uuid4().hex[:16]
    parts = []
    for k, v in fields.items():
        parts.append(('--' + b, 'Content-Disposition: form-data; name="%s"' % k, '', v))
    body = b''
    for head1, head2, head3, payload in parts:
        body += (head1 + '\r\n' + head2 + head3 + '\r\n\r\n' + payload + '\r\n').encode()
    body += ('--' + b + '\r\n' + 'Content-Disposition: form-data; name="%s"; filename="%s"\r\n' % (fileField, filename)
             + 'Content-Type: application/octet-stream\r\n\r\n').encode() + content + ('\r\n--' + b + '--\r\n').encode()
    return 'multipart/form-data; boundary=' + b, body

def post(port, path, cookie, ctype, body):
    c = http.client.HTTPConnection(HOST, port, timeout=15)
    h = {'Content-Type': ctype}
    if cookie: h['Cookie'] = cookie
    c.request('POST', path, body=body, headers=h)
    r = c.getresponse(); data = r.read(); c.close()
    return r.status, data

def req(port, method, path, data=None, cookie=None, extra=None):
    c = http.client.HTTPConnection(HOST, port, timeout=15)
    h = dict(extra or {})
    if data is not None:
        data = json.dumps(data).encode() if not isinstance(data, bytes) else data
        h.setdefault('Content-Type', 'application/json')
    if cookie: h['Cookie'] = cookie
    c.request(method, path, body=data, headers=h)
    r = c.getresponse(); data = r.read(); c.close()
    return r.status, data

def main():
    target = smoke.fixture(PORT)
    log = open(target / 'att-probe.log', 'wb')
    proc = subprocess.Popen([str(smoke.ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=smoke.ROOT,
                            stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        for _ in range(60):
            if proc.poll() is not None:
                raise RuntimeError('server died: ' + open(target / 'att-probe.log', errors='replace').read()[-2000:])
            try:
                if smoke.request(PORT, 'GET', '/admin/login')[0] == 200: break
            except OSError: pass
            time.sleep(0.2)

        r = smoke.request
        s, h, b = r(PORT, 'POST', '/admin/login', {'username': smoke.USER,
            'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
        assert json.loads(b)['result'], b
        ck = h['Set-Cookie'].split(';')[0]

        # 会员（买家）注册登录
        buyer = 'att_buyer'
        payload = {'username': buyer, 'password': smoke.client_hash(buyer, smoke.PASSWORD)}
        r(PORT, 'POST', '/api/v1/register', payload)
        s, h, b = r(PORT, 'POST', '/api/v1/login', payload)
        bck = h['Set-Cookie'].split(';')[0]

        # ---- 后台上传（multipart，含中文与二进制内容） ----
        content = bytes(range(256)) * 4 + '中文内容测试'.encode()
        ctype, body = multipart({'modelName': 'probe'}, 'file', '测试 文件.zip', content)
        s, b = post(PORT, '/admin/attachment/upload', ck, ctype, body)
        ret = json.loads(b)
        assert ret['result'], b
        admin_xid = ret['data']['xid']
        assert ret['data']['size'] == len(content), ret

        # 列表 / 详情 / 统计
        s, b = req(PORT, 'GET', '/admin/attachment/list?page=1&limit=10', cookie=ck)
        assert json.loads(b)['count'] >= 1 and any(x['xid'] == admin_xid for x in json.loads(b)['data']), b
        s, b = req(PORT, 'GET', '/admin/attachment/get?xid=' + admin_xid, cookie=ck)
        assert json.loads(b)['result'] and json.loads(b)['data']['ext'] == 'zip', b
        s, b = req(PORT, 'GET', '/admin/attachment/stats', cookie=ck)
        assert json.loads(b)['result'] and json.loads(b)['data']['total']['count'] >= 1, b

        # 保存（改访问控制为登录可见）
        s, b = req(PORT, 'POST', '/admin/attachment/save',
                   {'xid': admin_xid, 'allowHotlink': False, 'accessType': 1, 'accessLevel': 0,
                    'price': 0, 'priceType': 0, 'remark': '探针备注'}, cookie=ck)
        assert json.loads(b)['result'], b

        # 匿名访问登录附件 → 401
        s, b = req(PORT, 'GET', '/attachment?xid=' + admin_xid)
        assert s == 401, (s, b)
        # 会员登录后可取，内容逐字节一致
        s, b = req(PORT, 'GET', '/attachment?xid=' + admin_xid, cookie=bck)
        assert s == 200 and b == content, (s, len(b))
        # 防盗链：外站 Referer → 403
        s, b = req(PORT, 'GET', '/attachment?xid=' + admin_xid, cookie=bck, extra={'Referer': 'http://evil.example.com/x'})
        assert s == 403, (s, b)

        # ---- 会员上传付费附件 ----
        seller = 'att_seller'
        payload2 = {'username': seller, 'password': smoke.client_hash(seller, smoke.PASSWORD)}
        r(PORT, 'POST', '/api/v1/register', payload2)
        s, h, b = r(PORT, 'POST', '/api/v1/login', payload2)
        sck = h['Set-Cookie'].split(';')[0]

        paid_content = b'PAID-CONTENT-' + uuid.uuid4().hex.encode()
        ctype, body = multipart({'modelName': 'shop', 'accessType': '2', 'price': '10', 'priceType': '0'},
                                'file', 'paid.zip', paid_content)
        s, b = post(PORT, '/api/v1/attachment/upload', sck, ctype, body)
        ret = json.loads(b)
        assert ret['result'], b
        paid_xid = ret['data']['xid']

        # 买家余额不足（新会员 0 余额）→ 拒绝
        s, b = req(PORT, 'POST', '/api/v1/attachment/purchase', {'xid': paid_xid}, cookie=bck)
        assert not json.loads(b)['result'], b
        # 充值（直改库模拟），卖家分成为 90%
        import sqlite3
        with sqlite3.connect(target / 'db/main.db') as db:
            db.execute('UPDATE member SET balance = 100 WHERE username = ?', (buyer,))
            db.commit()
            seller_id = db.execute('SELECT id FROM member WHERE username = ?', (seller,)).fetchone()[0]
            buyer_id = db.execute('SELECT id FROM member WHERE username = ?', (buyer,)).fetchone()[0]
        # 未购买访问付费附件 → 402
        s, b = req(PORT, 'GET', '/attachment?xid=' + paid_xid, cookie=bck)
        assert s == 402 and json.loads(b)['price'] == 10, (s, b)
        # 购买 → 成功，余额 90，卖家 +9
        s, b = req(PORT, 'POST', '/api/v1/attachment/purchase', {'xid': paid_xid}, cookie=bck)
        assert json.loads(b)['result'], b
        with sqlite3.connect(target / 'db/main.db') as db:
            bal = db.execute('SELECT balance FROM member WHERE id = ?', (buyer_id,)).fetchone()[0]
            sbal = db.execute('SELECT balance FROM member WHERE id = ?', (seller_id,)).fetchone()[0]
            orders = db.execute('SELECT COUNT(*) FROM attachmentOrder WHERE attachmentXid = ?', (paid_xid,)).fetchone()[0]
        # platformFeeRate=20（attachment.json）→ 卖家分成 80%
        assert bal == 90 and sbal == 8 and orders == 1, (bal, sbal, orders)
        # 重复购买 → Already purchased
        s, b = req(PORT, 'POST', '/api/v1/attachment/purchase', {'xid': paid_xid}, cookie=bck)
        assert json.loads(b)['result'], b
        # 已购后可下载
        s, b = req(PORT, 'GET', '/attachment?xid=' + paid_xid, cookie=bck)
        assert s == 200 and b == paid_content, (s, len(b))
        # 卖家（上传者本人）无需购买
        s, b = req(PORT, 'GET', '/attachment?xid=' + paid_xid, cookie=sck)
        assert s == 200, (s, b)

        # 我的 / 已购列表
        s, b = req(PORT, 'GET', '/api/v1/attachment/my?page=1&limit=10', cookie=sck)
        assert any(x['xid'] == paid_xid for x in json.loads(b)['data']), b
        s, b = req(PORT, 'GET', '/api/v1/attachment/purchased?page=1&limit=10', cookie=bck)
        assert any(x['xid'] == paid_xid for x in json.loads(b)['data']), b

        # 后台删除 → 文件与记录消失
        s, b = req(PORT, 'DELETE', '/admin/attachment/delete?xid=' + admin_xid, cookie=ck)
        assert json.loads(b)['result'], b
        s, b = req(PORT, 'GET', '/attachment?xid=' + admin_xid, cookie=bck)
        assert s == 404, (s, b)

        # 视图页
        for p in ('/admin/view/attachment', '/admin/view/attachment/upload', '/admin/view/attachment/edit', '/admin/view/attachment/stats'):
            assert req(PORT, 'GET', p, cookie=ck)[0] == 200, p
        print('ATTACHMENT PROBE PASS')
    finally:
        proc.terminate()
        try: proc.wait(timeout=10)
        except Exception: proc.kill()
        print('fixture:', target)

if __name__ == '__main__':
    main()
