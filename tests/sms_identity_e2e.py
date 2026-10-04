"""Registered SMS through real account APIs; isolated DB, no external delivery."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, CSRF, client_hash, fixture, request


def run(args):
    original = hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest()
    target = fixture(args.port, source_db=args.source_db)
    dbpath = target/'db/main.db'
    cfg = json.loads((target/'xs.json').read_text())
    cfg['services'][0]['host_default']['devfile'] = str(ROOT/'tests/sms_identity_host.c')
    (target/'xs.json').write_text(json.dumps(cfg))
    sms = {'enabled': True, 'provider': 'webhook', 'options': {'url': 'https://sms.example.test/send', 'token': 'test-sms-secret'},
           'templates': {'verification': {'type': 'verification', 'parameters': ['code', 'minutes', 'purpose', 'expires_in']},
                         'order_update': {'type': 'notification', 'parameters': ['order']},
                         'contact_changed': {'type': 'notification', 'parameters': []}}}
    (target/'db/identity.json').write_text(json.dumps({'default_country_code': '+86', 'sms': sms}))
    log = target/'server.log'
    with log.open('wb') as out:
        process = subprocess.Popen([str(args.exe.resolve()), str(target/'xs.json')], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    def call(method, path, data=None, cookie=None, status=200, headers=None):
        actual, response_headers, body = request(args.port, method, path, data, cookie, headers)
        value = json.loads(body)
        assert actual == status and value['code'] == (0 if status < 400 else status), (path, actual, value)
        data = value.get('data')
        if isinstance(data, dict) and data.get('csrf_token') and response_headers.get('Set-Cookie', '').startswith('MSID='):
            CSRF[response_headers['Set-Cookie'].split(';')[0]] = data['csrf_token']
        return data, response_headers

    def sql(statement, values=()):
        with sqlite3.connect(dbpath) as db:
            result = db.execute(statement, values).fetchall()
            db.commit()
            return result

    def challenge(cookie=None, purpose='login', status=202):
        sql('UPDATE identity_rate SET expires_at=0')
        before = len(call('GET', '/__test/sms')[0])
        result, _ = call('POST', '/api/v1/profile/contacts/challenge' if cookie else '/api/v1/auth/challenges',
            {'channel': 'phone', 'target': '13800138000', 'purpose': purpose}, cookie, status)
        deliveries = call('GET', '/__test/sms')[0]
        assert len(deliveries) == before + 1
        if result is None:
            result = {'challenge_id': sql('SELECT id FROM identity_challenge ORDER BY rowid DESC LIMIT 1')[0][0], 'delivery': 'failed'}
        return {'challenge_id': result['challenge_id'], 'code': deliveries[-1]['code']}, result

    try:
        for _ in range(80):
            if process.poll() is not None: raise RuntimeError('xs exited')
            try:
                if request(args.port, 'GET', '/api/v1/auth/providers')[0] == 200: break
            except OSError: pass
            time.sleep(.2)
        else: raise RuntimeError('readiness timeout')
        assert call('GET', '/api/v1/auth/providers')[0]['phone_verification']
        call('POST', '/api/v1/register', {'username': 'sms_member', 'password': PASSWORD}, status=201)
        _, headers = call('POST', '/api/v1/login', {'identifier': 'sms_member', 'password': PASSWORD})
        cookie = headers['Set-Cookie'].split(';')[0]
        code, result = challenge(cookie)
        assert result['delivery'] == 'sent'
        call('POST', '/api/v1/profile/contacts/confirm', {**code, 'code': '000000' if code['code'] != '000000' else '111111'}, cookie, status=401)
        call('POST', '/api/v1/profile/contacts/confirm', code, cookie)
        call('POST', '/api/v1/profile/contacts/confirm', code, cookie, status=401)
        assert call('GET', '/api/v1/profile', cookie=cookie)[0]['phone_verified']
        assert sql('SELECT code_hash FROM identity_challenge WHERE id=?', (code['challenge_id'],))[0][0] != code['code']
        code, _ = challenge()
        _, headers = call('POST', '/api/v1/auth/challenges/verify', code)
        otp_cookie = headers['Set-Cookie'].split(';')[0]
        call('GET', '/api/v1/profile', cookie=otp_cookie)
        print('PASS registered SMS generation, binding, user-entered verification, one-time consumption and phone login')
        call('POST', '/__test/sms', {'result': 'unknown'})
        code, result = challenge()
        assert result['delivery'] == 'unknown'
        call('POST', '/api/v1/auth/challenges/verify', code)
        call('POST', '/__test/sms', {'result': 'failed'})
        code, result = challenge(status=502)
        assert result['delivery'] == 'failed'
        call('POST', '/api/v1/auth/challenges/verify', code, status=401)
        call('POST', '/__test/sms', {'result': 'accepted'})
        assert call('POST', '/__test/sms/notify', {})[0]['status'] == 1
        before = len(call('GET', '/__test/sms')[0])
        _, _, absent = request(args.port, 'POST', '/api/v1/sms/send', {})
        assert not absent.startswith(b'{"code":0') and len(call('GET', '/__test/sms')[0]) == before
        print('PASS uncertain submission without resend, explicit failures and template notifications without a public send API')
        status, headers, body = request(args.port, 'POST', '/admin/login', {'username': USER, 'password': client_hash(USER, PASSWORD)})
        assert status == 200 and json.loads(body)['result']
        admin = headers['Set-Cookie'].split(';')[0]
        settings, _ = call('GET', '/admin/member/identity/config', cookie=admin)
        assert len(settings['sms_providers']) == 9 and settings['sms']['secrets_configured']['token']
        assert 'test-sms-secret' not in json.dumps(settings)
        csrf = {'X-CSRF-Token': settings['csrf_token']}
        patch = {'sms': {'options': {'url': 'https://new.example.test/send'}}}
        call('PUT', '/admin/member/identity/config', {'revision': settings['revision'], 'config': patch}, admin, headers=csrf)
        stored = json.loads((target/'db/identity.json').read_text())
        assert stored['sms']['options']['token'] == 'test-sms-secret'
        settings, _ = call('GET', '/admin/member/identity/config', cookie=admin)
        assert settings['reload_required']
        call('PUT', '/admin/member/identity/config', {'revision': settings['revision'], 'config': {'sms': {'provider': 'unregistered'}}}, admin, status=400, headers=csrf)
        print('PASS platform catalog, private key preservation/redaction, saved configuration isolation and unknown-platform rejection')
        assert hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest() == original
    finally:
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)
        print(log.read_text(encoding='utf-8', errors='replace')[-2500:])
        print('Fixture:', target)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, default=ROOT/('xs.exe' if os.name == 'nt' else 'xs'))
    p.add_argument('--port', type=int, default=19294)
    p.add_argument('--source-db', type=Path)
    run(p.parse_args())
