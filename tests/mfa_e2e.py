"""MFA lifecycle and failure contracts against a disposable site, no real secrets."""
import argparse
import base64
import hashlib
import hmac
import http.client
from http.cookies import SimpleCookie
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import time
from urllib.parse import unquote,urlparse
from smoke import ROOT, USER, PASSWORD, client_hash, fixture


def totp(secret, future=0):
    counter = int(time.time() + future) // 30
    mac = hmac.new(base64.b32decode(secret), struct.pack('>Q', counter), hashlib.sha1).digest()
    offset = mac[-1] & 15
    return f'{(struct.unpack(">I", mac[offset:offset+4])[0] & 0x7fffffff) % 1000000:06d}'


class Client:
    def __init__(self, port):
        self.port, self.cookies, self.csrf = port, {}, ''
    def raw(self, method, path, data=None, headers=None):
        fields = {'Accept': 'application/json'}
        if self.cookies:
            fields['Cookie'] = '; '.join(f'{key}={value}' for key, value in self.cookies.items())
        if method != 'GET':
            token = self.csrf or self.cookies.get('MCSRF')
            if token:
                fields['X-CSRF-Token'] = token
        if data is not None:
            fields['Content-Type'] = 'application/json'
        fields.update(headers or {})
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=45)
        try:
            connection.request(method, path, None if data is None else json.dumps(data), fields)
            response = connection.getresponse()
            body = response.read()
            response_headers = response.getheaders()
            for key, value in response_headers:
                if key.lower() == 'set-cookie':
                    cookie = SimpleCookie();cookie.load(value)
                    for name, morsel in cookie.items():
                        if morsel['max-age'] == '0' or not morsel.value:
                            self.cookies.pop(name, None)
                        else:
                            self.cookies[name] = morsel.value
            return response.status, response_headers, body
        finally:
            connection.close()
    def api(self, method, path, data=None, status=200, headers=None):
        actual, response_headers, body = self.raw(method, path, data, headers)
        result = json.loads(body)
        assert actual == status and result['code'] == (0 if status < 400 else status), (path, actual, result)
        return result.get('data'), response_headers


def run(args):
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    key_existed = (ROOT / 'db/mfa.key').exists()
    target = fixture(args.port, protected=args.protected_entry)
    config = json.loads((target / 'xs.json').read_text())
    (target / 'tests').mkdir(exist_ok=True)
    (target / 'tests/mfa_host.c').write_text((ROOT / 'tests/mfa_host.c').read_text(), encoding='utf-8')
    config['services'][0]['host_default']['devfile'] = str(target / 'tests/mfa_host.c')
    (target / 'xs.json').write_text(json.dumps(config))
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': f'http://127.0.0.1:{args.port}'}))
    database, log = target / 'db/main.db', target / 'server.log'
    login_path = '/smoke-private-entry' if args.protected_entry else '/admin/login'
    process = None

    def sql(statement, values=()):
        with sqlite3.connect(database) as db:
            cursor = db.execute(statement, values)
            rows = cursor.fetchall();db.commit();return rows
    def launch():
        with log.open('ab') as output:
            return subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    def stop():
        if process and process.poll() is None:
            process.terminate()
            try: process.wait(timeout=10)
            except subprocess.TimeoutExpired: process.kill();process.wait(timeout=10)
    def ready():
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError('xs exited: ' + log.read_text(encoding='utf-8', errors='replace')[-5000:])
            try:
                if Client(args.port).raw('GET', login_path)[0] == 200:
                    return
            except OSError:
                pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')
    def admin_login(client, challenge=None, code=None):
        body = {'challenge_id': challenge, 'code': code} if challenge else {
            'username': USER, 'password': client_hash(USER, PASSWORD), 'remember': 'on'}
        status, headers, raw = client.raw('POST', login_path, body)
        result = json.loads(raw)
        assert status == 200, (status, result)
        assert any(key.lower()=='cache-control' and 'no-store'in value for key,value in headers)
        return result, headers
    def bearer(token):
        return {'Authorization': 'Bearer ' + token}

    try:
        process = launch();ready()
        admin = Client(args.port);result, _ = admin_login(admin)
        assert result['result']
        admin.api('GET', '/__test/mfa/invariants')
        assert admin.raw('GET', '/admin/view/auth/mfa')[0] == 200
        data, _ = admin.api('GET', '/admin/auth/mfa');admin.csrf = data['csrf_token']
        assert not data['enabled']
        member = Client(args.port)
        member.api('POST', '/api/v1/register', {'username': 'mfa_member', 'password': PASSWORD}, status=201)
        first, _ = member.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        owner = first['id']
        other = Client(args.port);second, _ = other.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        member.api('POST', '/api/v1/profile/mfa/setup', {}, status=403, headers={'X-CSRF-Token': ''})
        setup, _ = member.api('POST', '/api/v1/profile/mfa/setup', {'password': PASSWORD})
        assert setup['otpauth_uri'].startswith('otpauth://totp/') and '<svg' in setup['qr_svg']
        assert len((target / 'db/mfa.key').read_bytes()) == 32
        stored = sql('SELECT secret,hash FROM mfa_setup WHERE owner=?', (owner,))[0]
        assert len(stored[0]) == 48 and stored[1] != setup['setup_id']
        assert base64.b32decode(setup['secret']) not in stored[0]
        member.api('POST', '/api/v1/profile/mfa/confirm', {'setup_id': setup['setup_id'], 'code': 'wrong'}, status=401)
        enabled, _ = member.api('POST', '/api/v1/profile/mfa/confirm', {'setup_id': setup['setup_id'], 'code': totp(setup['secret'])})
        codes = enabled['recovery_codes'];assert len(codes) == len(set(codes)) == 10
        Client(args.port).api('GET', '/api/v1/profile', headers=bearer(first['access_token']), status=401)
        Client(args.port).api('GET', '/api/v1/profile', headers=bearer(second['access_token']), status=401)
        Client(args.port).api('POST', '/api/v1/token/refresh', {'refresh_token': first['refresh_token']}, status=401)
        assert not any(code in str(sql('SELECT hash FROM mfa_recovery')) for code in codes)
        print('PASS RFC vectors, AEAD realm/owner/version isolation, QR enrollment and old credential revocation')

        login = Client(args.port)
        before = sql('SELECT count(*) FROM member_session')[0][0]
        pending, _ = login.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        assert pending['mfa_required'] and not any(key in pending for key in ('access_token','refresh_token','balance','username','nickname')) and 'MSID' not in login.cookies
        assert sql('SELECT count(*) FROM member_session')[0][0] == before
        wrong_realm = Client(args.port)
        wrong_realm.raw('POST', login_path, {'challenge_id': pending['challenge_id'], 'code': codes[0]})
        assert 'XSID' not in wrong_realm.cookies
        login.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending['challenge_id'], 'code': 'invalid'}, status=401)
        login.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending['challenge_id'], 'code': totp(setup['secret'])}, status=401)
        signed_in, _ = login.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending['challenge_id'], 'code': totp(setup['secret'], 30)})
        login.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending['challenge_id'], 'code': codes[0]}, status=401)
        claims = json.loads(base64.urlsafe_b64decode(signed_in['access_token'].split('.')[1] + '=='))
        verified_at = sql('SELECT mfa_verified_at FROM member_session WHERE sid=?', (claims['sid'],))[0][0]
        refreshed, _ = login.api('POST', '/api/v1/token/refresh', {'refresh_token': signed_in['refresh_token']})
        assert sql('SELECT mfa_verified_at FROM member_session WHERE sid=?', (claims['sid'],))[0][0] == verified_at
        recovery = Client(args.port)
        pending2, _ = recovery.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        recovered, _ = recovery.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending2['challenge_id'], 'recovery_code': codes[0]})
        reuse = Client(args.port)
        pending3, _ = reuse.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        reuse.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending3['challenge_id'], 'code': codes[0]}, status=401)
        reuse.api('POST', '/api/v1/auth/mfa/cancel', {'challenge_id': pending3['challenge_id']})
        assert 'MMFA' not in reuse.cookies
        print('PASS no pre-MFA sessions/tokens, challenge and TOTP replay rejection, one-use recovery codes and refresh assurance')

        # Commit failures must preserve the old recovery set and active session.
        remaining = sql('SELECT count(*) FROM mfa_recovery WHERE owner=? AND used_at=0', (owner,))[0][0]
        sql("CREATE TRIGGER mfa_test_failure BEFORE INSERT ON mfa_recovery BEGIN SELECT RAISE(ABORT,'test failure'); END")
        recovery.api('POST', '/api/v1/profile/mfa/recovery-codes', {}, status=500)
        assert sql('SELECT count(*) FROM mfa_recovery WHERE owner=? AND used_at=0', (owner,))[0][0] == remaining
        recovery.api('GET', '/api/v1/profile')
        sql('DROP TRIGGER mfa_test_failure')
        regenerated, _ = recovery.api('POST', '/api/v1/profile/mfa/recovery-codes', {})
        Client(args.port).api('GET', '/api/v1/profile', headers=bearer(recovered['access_token']), status=401)
        new_codes = regenerated['recovery_codes']
        invalid_old = Client(args.port);pending4, _ = invalid_old.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        invalid_old.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending4['challenge_id'], 'code': codes[1]}, status=401)
        invalid_old.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': pending4['challenge_id'], 'code': new_codes[0]})
        # Recent MFA cannot be recreated by just resetting primary proof.
        sql('UPDATE member_session SET mfa_verified_at=? WHERE member_id=?', (int(time.time()) - 301, owner))
        invalid_old.api('POST', '/api/v1/profile/reauth', {'password': PASSWORD}, status=403)
        invalid_old.api('POST', '/api/v1/profile/mfa/reauth', {'password': PASSWORD}, status=403)
        invalid_old.api('POST', '/api/v1/profile/mfa/reauth', {'password': PASSWORD, 'code': new_codes[1]})
        replaced, _ = invalid_old.api('POST', '/api/v1/profile/mfa/setup', {})
        old_blob = sql("SELECT secret FROM mfa_factor WHERE realm='member' AND owner=?", (owner,))[0][0]
        assert old_blob == sql("SELECT secret FROM mfa_factor WHERE realm='member' AND owner=?", (owner,))[0][0]
        rebound, _ = invalid_old.api('POST', '/api/v1/profile/mfa/confirm', {'setup_id': replaced['setup_id'], 'code': totp(replaced['secret'])})
        assert old_blob != sql("SELECT secret FROM mfa_factor WHERE realm='member' AND owner=?", (owner,))[0][0]
        assert len(rebound['recovery_codes']) == 10
        print('PASS transactional failure rollback, recovery regeneration, step-up requirement and verifier replacement')

        # Persist an unfinished login across a process restart.
        waiting = Client(args.port);waiting_data, _ = waiting.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        stop();process = launch();ready()
        waiting.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': waiting_data['challenge_id'], 'code': rebound['recovery_codes'][0]})
        waiting.api('GET', '/api/v1/profile')
        # Five failures lock the factor even if a fresh primary challenge is used.
        attacked = Client(args.port);attack, _ = attacked.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        for _ in range(5):
            attacked.api('POST', '/api/v1/auth/mfa/verify', {'challenge_id': attack['challenge_id'], 'code': '00000x'}, status=401)
        assert sql("SELECT locked_until FROM mfa_factor WHERE realm='member' AND owner=?", (owner,))[0][0] > time.time()
        stop();process = launch();ready()
        assert sql("SELECT locked_until FROM mfa_factor WHERE realm='member' AND owner=?", (owner,))[0][0] > time.time()
        Client(args.port).api('POST','/api/v1/login',{'identifier':'mfa_member','password':PASSWORD},status=429)
        sql("UPDATE mfa_factor SET locked_until=0,failures=0 WHERE realm='member' AND owner=?", (owner,))
        waiting.api('POST', '/api/v1/profile/mfa/disable', {})
        normal, _ = waiting.api('POST', '/api/v1/login', {'identifier': 'mfa_member', 'password': PASSWORD})
        assert 'access_token' in normal and not normal.get('mfa_required')
        print('PASS persistent challenges, bindings, sessions and account cooldown; disabling rotates credentials')

        # Backend realm has independent factors, CSRF and the original private entry.
        admin = Client(args.port);result, _ = admin_login(admin);assert result['result']
        state, _ = admin.api('GET', '/admin/auth/mfa');admin.csrf = state['csrf_token']
        admin.api('POST', '/admin/auth/mfa/setup', {}, status=403, headers={'X-CSRF-Token': ''})
        admin_setup, _ = admin.api('POST', '/admin/auth/mfa/setup', {})
        old_xsid = admin.cookies['XSID']
        activated, _ = admin.api('POST', '/admin/auth/mfa/confirm', {'setup_id': admin_setup['setup_id'], 'code': totp(admin_setup['secret'])})
        assert admin.cookies['XSID'] != old_xsid
        admin.csrf = ''
        state, _ = admin.api('GET', '/admin/auth/mfa');admin.csrf = state['csrf_token'];assert state['enabled']
        old_admin = Client(args.port);old_admin.cookies['XSID'] = old_xsid
        assert old_admin.raw('GET', '/admin')[0] != 200
        new_admin = Client(args.port);challenge, _ = admin_login(new_admin)
        assert challenge['mfa_required'] and not challenge['result'] and 'XSID' not in new_admin.cookies
        assert new_admin.raw('GET', '/admin')[0] != 200
        result, headers = admin_login(new_admin, challenge['challenge_id'], activated['recovery_codes'][0])
        assert result['result'] and 'XSID' in new_admin.cookies
        assert any('Max-Age=604800' in value for key, value in headers if key.lower() == 'set-cookie')
        state, _ = new_admin.api('GET', '/admin/auth/mfa');new_admin.csrf = state['csrf_token']
        new_admin.api('POST', '/admin/auth/mfa/disable', {})
        assert not new_admin.api('GET', '/admin/auth/mfa')[0]['enabled']
        print('PASS backend enrollment, CSRF, cookie rotation, MFA challenge and remember-login contract')

        ordinary=Client(args.port);name='MFA_'+'测试🔐'*20
        salt='mfa-test-only-salt'
        digest=hashlib.sha256((name+salt+client_hash(name,PASSWORD)).encode()).hexdigest().upper()
        sql('UPDATE user SET user=?,salt=?,pwd=? WHERE user=?',(name,salt,digest,USER+'_denied'))
        actual,_,body=ordinary.raw('POST',login_path,{'username':name,'password':client_hash(name,PASSWORD)})
        assert actual==200 and json.loads(body)['result']
        assert ordinary.raw('GET','/admin/view/auth/mfa')[0]==200
        state,_=ordinary.api('GET','/admin/auth/mfa');ordinary.csrf=state['csrf_token']
        ordinary_setup,_=ordinary.api('POST','/admin/auth/mfa/setup',{})
        assert unquote(urlparse(ordinary_setup['otpauth_uri']).path).endswith(name) and '<svg'in ordinary_setup['qr_svg']
        ordinary.api('POST','/admin/auth/mfa/confirm',{'setup_id':ordinary_setup['setup_id'],'code':totp(ordinary_setup['secret'])})
        own_id=sql('SELECT id FROM user WHERE user=?',(name,))[0][0]
        assert sql("SELECT enabled FROM mfa_factor WHERE realm='admin'AND owner=?",(own_id,))==[(1,)]
        assert not new_admin.api('GET','/admin/auth/mfa')[0]['enabled']
        print('PASS long Unicode admin QR enrollment and own MFA without user-management permission')

        # Emergency recovery has no HTTP switch and requires an explicit local target.
        member_setup,_=waiting.api('POST','/api/v1/profile/mfa/setup',{'password':PASSWORD})
        local_member,_=waiting.api('POST','/api/v1/profile/mfa/confirm',{'setup_id':member_setup['setup_id'],'code':totp(member_setup['secret'])})
        state,_=new_admin.api('GET','/admin/auth/mfa');new_admin.csrf=state['csrf_token']
        setup_admin,_=new_admin.api('POST','/admin/auth/mfa/setup',{})
        new_admin.api('POST','/admin/auth/mfa/confirm',{'setup_id':setup_admin['setup_id'],'code':totp(setup_admin['secret'])})
        admin_id=sql('SELECT id FROM user WHERE user=?',(USER,))[0][0]
        pending_member=Client(args.port)
        pending_member.api('POST','/api/v1/login',{'identifier':'mfa_member','password':PASSWORD})
        pending_admin=Client(args.port);admin_login(pending_admin)
        primary_before=(sql('SELECT pwd FROM user WHERE id=?',(admin_id,)),sql('SELECT pwd FROM member WHERE id=?',(owner,)))
        stop()
        def local_tool(realm,account,apply=False):
            command=[sys.executable,'-X','utf8',str(ROOT/'tools/mfa_recover.py'),'--db',str(database),'--realm',realm,'--account-id',str(account)]
            if apply:command+=['--apply','--reason','isolated recovery regression']
            return subprocess.run(command,capture_output=True,text=True,encoding='utf-8')
        before=hashlib.sha256(database.read_bytes()).digest()
        preview=local_tool('member',owner)
        assert preview.returncode==0 and not json.loads(preview.stdout)['applied']
        assert hashlib.sha256(database.read_bytes()).digest()==before
        sql("CREATE TRIGGER mfa_offline_failure BEFORE INSERT ON mfa_audit BEGIN SELECT RAISE(ABORT,'test failure'); END")
        assert local_tool('member',owner,True).returncode==1
        assert sql("SELECT enabled FROM mfa_factor WHERE realm='member'AND owner=?",(owner,))==[(1,)]
        assert sql("SELECT count(*)FROM mfa_recovery WHERE realm='member'AND owner=?",(owner,))==[(10,)]
        sql('DROP TRIGGER mfa_offline_failure')
        for realm,account in (('member',owner),('admin',admin_id)):
            result=local_tool(realm,account,True);assert result.returncode==0,result.stderr
            assert json.loads(result.stdout)['applied']
            assert sql('SELECT enabled,secret FROM mfa_factor WHERE realm=?AND owner=?',(realm,account))==[(0,None)]
            assert sql('SELECT count(*)FROM mfa_challenge WHERE realm=?AND owner=?',(realm,account))==[(0,)]
            assert sql('SELECT count(*)FROM mfa_recovery WHERE realm=?AND owner=?',(realm,account))==[(0,)]
        assert primary_before==(sql('SELECT pwd FROM user WHERE id=?',(admin_id,)),sql('SELECT pwd FROM member WHERE id=?',(owner,)))
        process=launch();ready()
        Client(args.port).api('GET','/api/v1/profile',headers=bearer(local_member['access_token']),status=401)
        assert admin_login(Client(args.port))[0]['result']
        assert not Client(args.port).api('POST','/api/v1/login',{'identifier':'mfa_member','password':PASSWORD})[0].get('mfa_required')
        print('PASS local emergency recovery preview, transaction rollback, per-realm disable, session revocation and unchanged passwords')

        # Losing or replacing the private key must fail closed, never silently disable MFA.
        stop()
        key = target / 'db/mfa.key';key_bytes = key.read_bytes();key.write_bytes(bytes(32))
        process = launch()
        for _ in range(60):
            try:
                response = Client(args.port).raw('GET', login_path)
                assert response[0] == 503, response[0];break
            except OSError:
                if process.poll() is not None:break
                time.sleep(.2)
        assert 'refusing startup' in log.read_text(encoding='utf-8', errors='replace')
        stop();key.write_bytes(key_bytes);process = launch();ready()
        content = log.read_text(encoding='utf-8', errors='replace')
        assert '[tcc]' not in content, content[-5000:]
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original
        assert (ROOT / 'db/mfa.key').exists() == key_existed
        print('PASS private-key mismatch refuses service; zero compiler warnings; real user database unchanged')
    finally:
        stop()
        print('Fixture:', target)
        if log.exists() and process and process.returncode not in (0, 1, -15):
            print(log.read_text(encoding='utf-8', errors='replace')[-5000:])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
    parser.add_argument('--port', type=int, default=19214)
    parser.add_argument('--protected-entry', action='store_true')
    run(parser.parse_args())
