# -*- coding: utf-8 -*-
"""notify 家族功能探针：管理员发送 → 会员列表/未读/详情/已读/删除 + 管理列表。"""
import sys, json, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import smoke

PORT = 19301

def main():
    target = smoke.fixture(PORT)
    log = open(target / 'notify-probe.log', 'wb')
    import subprocess
    proc = subprocess.Popen([str(smoke.ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=smoke.ROOT,
                            stdout=log, stderr=subprocess.STDOUT,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        for _ in range(60):
            if proc.poll() is not None:
                raise RuntimeError('server died: ' + open(target / 'notify-probe.log', errors='replace').read()[-2000:])
            try:
                if smoke.request(PORT, 'GET', '/admin/login')[0] == 200:
                    break
            except OSError:
                pass
            time.sleep(0.2)

        r = smoke.request
        # 管理员登录
        s, h, b = r(PORT, 'POST', '/admin/login', {'username': smoke.USER,
            'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
        assert json.loads(b)['result'], b
        ck = h['Set-Cookie'].split(';')[0]

        # 会员注册并登录
        member = 'notify_probe'
        payload = {'username': member, 'password': smoke.client_hash(member, smoke.PASSWORD)}
        s, _, b = r(PORT, 'POST', '/api/v1/register', payload)
        assert json.loads(b)['code'] == 0, b
        s, h, b = r(PORT, 'POST', '/api/v1/login', payload)
        assert json.loads(b)['code'] == 0, b
        mc = h['Set-Cookie'].split(';')[0]

        # 未登录访问应 401
        assert r(PORT, 'GET', '/api/v1/notify/list')[0] == 401

        # 初始未读 0
        s, _, b = r(PORT, 'GET', '/api/v1/notify/unread_count', cookie=mc)
        assert json.loads(b)['data']['unread'] == 0, b

        # 管理员发送（按用户定向）
        # 取真实会员 id
        s, _, b = r(PORT, 'GET', '/admin/member/user?page=1&limit=10&search=' + member, cookie=ck)
        mid = json.loads(b)['data'][0]['id']
        s, _, b = r(PORT, 'POST', '/admin/member/notify', {
            'sendType': 'users', 'memberIds': str(mid),
            'title': '探针标题', 'content': '探针内容', 'actionUrl': '/x'}, cookie=ck)
        ret = json.loads(b)
        assert ret['result'] and ret['data']['recipientCount'] == 1, b

        # 未读 1 → 列表 → 详情 → 已读 → 未读 0 → 删除
        s, _, b = r(PORT, 'GET', '/api/v1/notify/unread_count', cookie=mc)
        assert json.loads(b)['data']['unread'] == 1, b
        s, _, b = r(PORT, 'GET', '/api/v1/notify/list', cookie=mc)
        lst = json.loads(b)
        assert lst['code'] == 0 and lst['count'] == 1 and lst['data'][0]['title'] == '探针标题', b
        rid = lst['data'][0]['id']
        s, _, b = r(PORT, 'GET', '/api/v1/notify/detail?id=%d' % rid, cookie=mc)
        assert json.loads(b)['data']['content'] == '探针内容', b
        s, _, b = r(PORT, 'GET', '/api/v1/notify/detail?id=%d' % (rid + 9999), cookie=mc)
        assert json.loads(b)['code'] == 404, b
        s, _, b = r(PORT, 'POST', '/api/v1/notify/read', {'id': rid}, cookie=mc)
        assert json.loads(b)['code'] == 0, b
        s, _, b = r(PORT, 'GET', '/api/v1/notify/unread_count', cookie=mc)
        assert json.loads(b)['data']['unread'] == 0, b
        # 管理列表可见
        s, _, b = r(PORT, 'GET', '/admin/member/notify?page=1&limit=10', cookie=ck)
        assert json.loads(b)['count'] == 1 and json.loads(b)['data'][0]['recipientCount'] == 1, b
        # 全部已读 + 删除
        s, _, b = r(PORT, 'POST', '/admin/member/notify', {
            'sendType': 'users', 'memberIds': str(mid),
            'title': '第二条', 'content': '内容2'}, cookie=ck)
        assert json.loads(b)['result'], b
        s, _, b = r(PORT, 'POST', '/api/v1/notify/read_all', {}, cookie=mc)
        assert json.loads(b)['code'] == 0, b
        s, _, b = r(PORT, 'GET', '/api/v1/notify/unread_count', cookie=mc)
        assert json.loads(b)['data']['unread'] == 0, b
        s, _, b = r(PORT, 'POST', '/api/v1/notify/delete', {'id': rid}, cookie=mc)
        assert json.loads(b)['code'] == 0, b
        s, _, b = r(PORT, 'GET', '/api/v1/notify/list', cookie=mc)
        assert json.loads(b)['count'] == 1, b  # 只剩第二条
        # 视图页可达
        assert r(PORT, 'GET', '/admin/view/member/notify', cookie=ck)[0] == 200
        assert r(PORT, 'GET', '/admin/view/member/notify/send', cookie=ck)[0] == 200
        # all 群发分支
        s, _, b = r(PORT, 'POST', '/admin/member/notify', {
            'sendType': 'all', 'title': '全员', 'content': '内容3'}, cookie=ck)
        assert json.loads(b)['result'] and json.loads(b)['data']['recipientCount'] >= 1, b
        # groups 分支（组 1）
        s, _, b = r(PORT, 'POST', '/admin/member/notify', {
            'sendType': 'groups', 'groupIds': '1', 'title': '按组', 'content': '内容4'}, cookie=ck)
        assert json.loads(b)['result'], b
        print('NOTIFY PROBE PASS')
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except Exception:
            proc.kill()
        print('fixture:', target)

if __name__ == '__main__':
    main()
