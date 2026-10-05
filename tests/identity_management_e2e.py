"""Account setup, private settings, key rotation and CORS; no load tests."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request


def run(args):
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    target = fixture(args.port)
    database = target / 'db/main.db'
    origin = f'http://127.0.0.1:{args.port}'
    config = json.loads((target / 'xs.json').read_text())
    config['services'][0]['host_default']['devfile'] = str(target / 'tests/identity_management_host.c')
    (target / 'xs.json').write_text(json.dumps(config))
    (target / 'db/identity.json').write_text(json.dumps({
        'public_origin': origin, 'cors_origins': ['https://client.example.com'],
        'sms_webhook_token': 'test-write-only-secret'}))
    log = target / 'server.log'

    def launch():
        with log.open('ab') as output:
            return subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    process = launch()

    def ready():
        for _ in range(80):
            if process.poll() is not None:
                raise RuntimeError('xs exited')
            try:
                if request(args.port, 'GET', '/admin/login')[0] == 200:
                    return
            except OSError:
                pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')

    def call(method, path, data=None, cookie=None, status=200, headers=None):
        actual, response_headers, body = request(args.port, method, path, data, cookie, headers)
        assert actual == status, (path, actual, body)
        result = json.loads(body) if body else None
        if result:
            assert result['code'] == (0 if status < 400 else status), (path, result)
        return result['data'] if result else None, response_headers

    def sql(statement, values=()):
        with sqlite3.connect(database) as db:
            cursor = db.execute(statement, values)
            rows = cursor.fetchall()
            db.commit()
            return rows

    def member_login(identifier='manage_member', password=PASSWORD):
        data, headers = call('POST', '/api/v1/login', {'identifier': identifier, 'password': password})
        return data, headers['Set-Cookie'].split(';')[0]

    def admin_login(user=USER):
        status, headers, body = request(args.port, 'POST', '/admin/login', {
            'username': user, 'password': client_hash(user, PASSWORD)})
        assert status == 200 and json.loads(body)['result'], body
        return headers['Set-Cookie'].split(';')[0]

    try:
        ready()
        call('POST', '/api/v1/register', {'username': 'manage_member', 'password': PASSWORD}, status=201)
        tokens, cookie = member_login()
        current, _ = call('GET', '/api/v1/session', cookie=cookie)
        assert len(current['csrf_token']) == 64 and current['session_id']
        # With the matching readable cookie, a refresh does not rotate CSRF.
        current2, _ = call('GET', '/api/v1/session', cookie=cookie + '; MCSRF=' + current['csrf_token'])
        assert current2['csrf_token'] == current['csrf_token']
        call('GET', '/api/v1/profile', headers={'Authorization': 'Bearer ' + current['access_token']})
        call('GET', '/api/v1/profile', headers={'Authorization': 'bEaReR   ' + current['access_token']})
        assert request(args.port, 'GET', '/account/index.html')[0] == 200
        print('PASS session bootstrap, stable CSRF across page refresh and account assets')

        _, other = member_login()
        call('POST', '/api/v1/profile/credentials', {'username': 'renamed_member', 'password': 'Changed-pass-2'}, cookie)
        call('GET', '/api/v1/profile', cookie=other, status=401)
        call('POST', '/api/v1/login', {'identifier': 'manage_member', 'password': PASSWORD}, status=401)
        tokens, cookie = member_login('renamed_member', 'Changed-pass-2')
        call('POST', '/api/v1/profile/credentials', {'username': '12345678', 'password': PASSWORD}, cookie, status=400)
        call('POST', '/api/v1/profile/credentials', {'username': 'user@example.com', 'password': PASSWORD}, cookie, status=400)
        # An external-only member can establish local credentials with recent proof.
        sql('UPDATE member SET username=NULL,salt=NULL,pwd=NULL WHERE id=?', (tokens['id'],))
        sql("INSERT INTO member_external_identity(member_id,provider,app_namespace,subject,created_at) VALUES(?,'github','disabled-app','101',0)", (tokens['id'],))
        sql("UPDATE member SET phone='+12025550123',phone_key='+12025550123',phone_verified_at=1,email='Case@example.com',email_key='Case@example.com',email_verified_at=1 WHERE id=?", (tokens['id'],))
        # Disabled SSO and unavailable OTP delivery cannot justify removing the
        # remaining contact. No provider call or message send occurs here.
        call('DELETE', '/api/v1/profile/contacts', {'channel': 'phone'}, cookie, status=409)
        call('POST', '/api/v1/profile/credentials', {'username': 'external_member', 'password': PASSWORD}, cookie)
        tokens, cookie = member_login('external_member')
        sql('UPDATE member_session SET reauth_until=0 WHERE sid=?', (current['session_id'],))
        # The newly issued cookie must itself expire proof, not an already revoked sid.
        sid = sql('SELECT sid FROM member_session WHERE cookie_hash=?', (hashlib.sha256(cookie[5:].encode()).hexdigest(),))[0][0]
        sql('UPDATE member_session SET reauth_until=0 WHERE sid=?', (sid,))
        call('POST', '/api/v1/profile/credentials', {'username': 'another_member', 'password': PASSWORD}, cookie, status=403)
        call('POST', '/api/v1/profile/reauth', {'password': PASSWORD}, cookie)
        print('PASS account naming, credential setup for external members and device revocation')

        allowed = {'Origin': 'https://client.example.com', 'Authorization': 'Bearer ' + tokens['access_token']}
        _, headers = call('GET', '/api/v1/profile', headers=allowed)
        assert headers['Access-Control-Allow-Origin'] == allowed['Origin'] and 'Access-Control-Allow-Credentials' not in headers
        call('PUT', '/api/v1/profile', {'nickname': 'Bearer client'}, headers=allowed)
        call('PUT', '/api/v1/profile', {'nickname': 'bad origin'}, headers={**allowed, 'Origin': 'https://untrusted.example.com'}, status=403)
        call('PUT', '/api/v1/profile', {'nickname': 'cookie cross-site'}, cookie, headers={'Origin': allowed['Origin']}, status=403)
        call('OPTIONS', '/api/v1/profile', status=204, headers={'Origin': allowed['Origin'], 'Access-Control-Request-Method': 'PUT', 'Access-Control-Request-Headers': 'Authorization, Content-Type'})
        call('OPTIONS', '/api/v1/profile', status=403, headers={'Origin': allowed['Origin'], 'Access-Control-Request-Method': 'PUT', 'Access-Control-Request-Headers': 'X-Unapproved'})
        call('OPTIONS', '/api/v1/profile', status=403, headers={'Origin': 'https://untrusted.example.com', 'Access-Control-Request-Method': 'GET'})
        print('PASS exact CORS allowlist, preflight header policy and no cross-site cookies')

        admin = admin_login()
        settings, _ = call('GET', '/admin/member/identity/config', cookie=admin)
        assert 'test-write-only-secret' not in json.dumps(settings)
        assert 'client_secret' not in settings['github'] and 'client_secret' not in settings['wechat']
        assert 'token' not in settings['sms']['options']
        assert settings['sms_token_configured'] and 'sms_webhook_token' not in settings
        assert settings['session_idle_days'] == 30 and settings['session_max_days'] == 0
        call('PUT', '/admin/member/identity/config', {'revision': settings['revision'], 'config': {'registration': False}}, admin, status=403)
        csrf = {'X-CSRF-Token': settings['csrf_token'], 'Origin': origin}
        before = (target / 'db/identity.json').read_bytes()
        for patch in ({'registration': 'false'}, {'public_origin': 'https://example.com/path'}, {'cors_origins': ['*']},
                      {'github': {'enabled': True}}, {'unexpected': True}, {'session_idle_days': 0},
                      {'session_idle_days': 1.5}, {'session_max_days': -1}, {'session_max_days': 3651}):
            call('PUT', '/admin/member/identity/config', {'revision': settings['revision'], 'config': patch}, admin, status=400, headers=csrf)
            assert (target / 'db/identity.json').read_bytes() == before
        call('PUT', '/admin/member/identity/config', {'revision': settings['revision'], 'config': {
            'registration': False, 'session_idle_days': 7, 'session_max_days': 90}}, admin, headers=csrf)
        stored = json.loads((target / 'db/identity.json').read_text(encoding='utf-8-sig'))
        assert stored['sms_webhook_token'] == 'test-write-only-secret'
        assert stored['session_idle_days'] == 7 and stored['session_max_days'] == 90
        call('PUT', '/admin/member/identity/config', {'revision': settings['revision'], 'config': {'registration': True}}, admin, status=409, headers=csrf)
        # Immutable generation: saved edits remain pending until application reload.
        assert call('GET', '/api/v1/auth/providers')[0]['registration']
        denied = admin_login(USER + '_denied')
        assert request(args.port, 'GET', '/admin/member/identity/config', cookie=denied)[0] == 403
        page_status, page_headers, _ = request(args.port, 'GET', '/admin/view/member/identity', cookie=admin)
        assert page_status == 200 and page_headers['Content-Type'].startswith('text/html')
        print('PASS private secret redaction/preservation, strict validation, stale edits and existing admin permissions')

        call('POST', '/admin/member/identity/keys/rotate', {}, admin, headers=csrf)
        call('GET', '/api/v1/profile', headers={'Authorization': 'Bearer ' + tokens['access_token']})
        assert sql('SELECT count(*) FROM identity_key WHERE active=1') == [(1,)]
        call('POST', '/admin/member/identity/keys/rotate', {'invalidate_existing': True}, admin, headers=csrf)
        call('GET', '/api/v1/profile', cookie=cookie, status=401)
        call('GET', '/api/v1/profile', headers={'Authorization': 'Bearer ' + tokens['access_token']}, status=401)
        assert sql('SELECT count(*) FROM identity_key') == [(1,)]
        # Maintenance drops expired families but keeps a live family's replay history.
        _, cookie = member_login('external_member')
        old = 'a' * 64
        sql("INSERT INTO member_session(sid,member_id,cookie_hash,csrf_hash,created_at,last_used,expires_at,reauth_until,revoked_at,ip,user_agent) VALUES(?,?,?, ?,0,0,0,0,0,'','')", (old, tokens['id'], 'b' * 64, 'c' * 64))
        sql('INSERT INTO member_refresh VALUES(?,?,1)', ('d' * 64, old))
        sql("INSERT INTO member_refresh SELECT ?,sid,1 FROM member_session WHERE cookie_hash=?", ('e' * 64, hashlib.sha256(cookie[5:].encode()).hexdigest()))
        call('POST', '/__test/identity/maintenance', {})
        assert not sql('SELECT sid FROM member_session WHERE sid=?', (old,))
        assert sql('SELECT hash FROM member_refresh WHERE hash=?', ('e' * 64,))
        process.terminate(); process.wait(timeout=10); process = launch(); ready()
        assert not call('GET', '/api/v1/auth/providers')[0]['registration']
        call('GET', '/api/v1/profile', cookie=cookie)
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original
        print('PASS normal/emergency key rotation, bounded retention and private settings after restart; real DB unchanged')
    except Exception:
        print(log.read_text(encoding='utf-8', errors='replace')[-5000:])
        raise
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait(timeout=10)
        print('Fixture:', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
    parser.add_argument('--port', type=int, default=19241)
    run(parser.parse_args())
