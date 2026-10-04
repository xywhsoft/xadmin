"""Public native login/PKCE regression using an isolated xs/SQLite fixture.

No Internet, production accounts, paid search, pressure or load tests.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from urllib.parse import parse_qs, urlencode, urlsplit
from smoke import ROOT, PASSWORD, fixture, request


def run(args):
    target = fixture(args.port, register_interval=0)
    database = target / 'db/main.db'
    origin = f'http://127.0.0.1:{args.port}'
    config = json.loads((target / 'xs.json').read_text())
    config['services'][0]['host_default']['devfile'] = str(ROOT / 'main.c')
    (target / 'xs.json').write_text(json.dumps(config))
    identity = {'public_origin': origin, 'applications': [
        {'client_id': 'example-desktop', 'name': 'Example App', 'redirect_uris': [
            'http://127.0.0.1:{port}/api/v1/account/callback',
            'http://[::1]:{port}/api/v1/account/callback']},
        {'client_id': 'example-mobile', 'name': 'Example Mobile', 'redirect_uris': [
            'https://example.test/app/callback']}]}
    (target / 'db/identity.json').write_text(json.dumps(identity))
    log = target / 'application-server.log'
    process = None

    def launch():
        with log.open('ab') as output:
            return subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    def ready():
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError(log.read_text(errors='replace')[-5000:])
            try:
                if request(args.port, 'GET', '/api/v1/auth/providers')[0] == 200: return
            except OSError: pass
            time.sleep(.15)
        raise RuntimeError(log.read_text(errors='replace')[-5000:])

    def call(method, path, data=None, cookie=None, status=200, **headers):
        actual, response, body = request(args.port, method, path, data, cookie, headers)
        assert actual == status, (path, actual, body)
        value = json.loads(body) if body else None
        if value: assert value['code'] == (0 if status < 400 else status), value
        return value['data'] if value else None, response

    def sql(statement, values=()):
        with sqlite3.connect(database) as db:
            return db.execute(statement, values).fetchall()

    verifier = '0123456789abcdef' * 4
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b'=').decode()
    redirect = 'http://127.0.0.1:45321/api/v1/account/callback'
    state = 'abcdef0123456789' * 4
    params = dict(client_id='example-desktop', redirect_uri=redirect, response_type='code', state=state,
                  code_challenge=challenge, code_challenge_method='S256')

    def begin(overrides=None, status=303, suffix=''):
        values = {**params, **(overrides or {})}
        _, response = call('GET', '/api/v1/auth/authorize?' + urlencode(values) + suffix, status=status)
        if status != 303: return None
        assert response['Cache-Control'] == 'no-store' and response['Referrer-Policy'] == 'no-referrer'
        app_cookie = response['Set-Cookie'].split(';')[0]
        assert app_cookie.startswith('XAPP=') and 'HttpOnly' in response['Set-Cookie']
        request_id = parse_qs(urlsplit(response['Location']).query)['application'][0]
        return request_id, app_cookie

    def approve(pending, cookie, csrf, approve=True, status=200):
        value, _ = call('POST', '/api/v1/auth/authorize', {'request_id': pending[0], 'approve': approve},
            cookie + '; ' + pending[1], status, **{'X-CSRF-Token': csrf, 'Origin': origin})
        return value

    def redeem(code, changes=None, status=200):
        value, response = call('POST', '/api/v1/auth/token', {'grant_type': 'authorization_code',
            'client_id': params['client_id'], 'redirect_uri': redirect, 'code': code,
            'code_verifier': verifier, **(changes or {})}, status=status)
        assert 'Set-Cookie' not in response
        return value

    try:
        process = launch(); ready()
        call('POST', '/api/v1/register', {'username': 'application_member', 'password': PASSWORD}, status=201)
        tokens, headers = call('POST', '/api/v1/login', {'identifier': 'application_member', 'password': PASSWORD})
        browser = headers['Set-Cookie'].split(';')[0]
        csrf = tokens['csrf_token']
        for changes in [dict(client_id='unknown'), dict(redirect_uri='https://evil.test/callback'),
            dict(redirect_uri='http://127.0.0.1:0/api/v1/account/callback'),
            dict(redirect_uri='http://127.0.0.1:045321/api/v1/account/callback'),
            dict(redirect_uri='http://localhost:45321/api/v1/account/callback'),
            dict(code_challenge_method='plain'), dict(state='short'), dict(code_challenge='x' * 42)]:
            begin(changes, 400)
        begin(status=400, suffix='&%63lient_id=example-desktop')
        pending = begin()
        call('GET', '/api/v1/auth/authorize?request_id=' + pending[0], status=410)
        context, _ = call('GET', '/api/v1/auth/authorize?request_id=' + pending[0], cookie=pending[1])
        assert context['name'] == 'Example App' and not context['signed_in']
        call('POST', '/api/v1/auth/authorize', {'request_id': pending[0], 'approve': True},
            pending[1], status=401, Origin=origin)
        call('POST', '/api/v1/auth/authorize', {'request_id': pending[0], 'approve': True},
            browser + '; ' + pending[1], status=403, **{'X-CSRF-Token': 'f' * 64, 'Origin': origin})
        call('POST', '/api/v1/auth/authorize', {'request_id': pending[0], 'approve': False},
            pending[1], status=403, Origin='https://evil.test')
        sql('UPDATE member_session SET reauth_until=0 WHERE member_id=?', (tokens['id'],))
        result = approve(pending, browser, csrf)
        returned = parse_qs(urlsplit(result['redirect_uri']).query)
        assert returned['state'] == [state] and set(returned) == {'code', 'state'}
        code = returned['code'][0]
        assert sql('SELECT code_hash FROM identity_application WHERE request_id=?', (pending[0],))[0][0] != code
        redeem(code, {'code_verifier': 'f' * 64}, 401)
        redeem(code, {'redirect_uri': 'http://127.0.0.1:45322/api/v1/account/callback'}, 401)
        # Approved codes survive a host restart; config and DB remain authoritative.
        process.terminate(); process.wait(timeout=10); process = launch(); ready()
        native = redeem(code)
        redeem(code, status=401)
        native_session, _ = call('GET', '/api/v1/session', Authorization='Bearer ' + native['access_token'])
        assert native_session['reauth_until'] == 0
        call('POST', '/api/v1/logout', Authorization='Bearer ' + native['access_token'])
        browser_session, _ = call('GET', '/api/v1/session', cookie=browser)
        csrf = browser_session['csrf_token']
        call('GET', '/api/v1/session', status=401, Authorization='Bearer ' + native['access_token'])
        cancelled = begin()
        result = approve(cancelled, browser, csrf, False)
        assert parse_qs(urlsplit(result['redirect_uri']).query)['error'] == ['access_denied']
        approve(cancelled, browser, csrf, status=410)
        revoked = begin()
        code = parse_qs(urlsplit(approve(revoked, browser, csrf)['redirect_uri']).query)['code'][0]
        call('POST', '/api/v1/logout', cookie=browser)
        redeem(code, status=401)
        expired = begin()
        sql('UPDATE identity_application SET expires_at=0 WHERE request_id=?', (expired[0],))
        call('GET', '/api/v1/auth/authorize?request_id=' + expired[0], cookie=expired[1], status=410)
        mobile = begin(dict(client_id='example-mobile', redirect_uri='https://example.test/app/callback'))
        call('GET', '/api/v1/auth/authorize?request_id=' + mobile[0], cookie=mobile[1])
        # No raw refresh token is retained in the native session database.
        assert sql('SELECT count(*) FROM member_refresh WHERE hash=?', (native['refresh_token'],)) == [(0,)]
        print('PASS public application login: PKCE, strict redirects/query, browser/CSRF binding, cancellation, restart, one-time codes, revocation and independent sessions')
    finally:
        if process and process.poll() is None: process.terminate(); process.wait(timeout=10)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
    parser.add_argument('--port', type=int, default=19441)
    run(parser.parse_args())
