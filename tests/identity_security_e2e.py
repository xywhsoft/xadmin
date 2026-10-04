"""Security status and administrator-assisted recovery, functional tests only.

Uses a disposable SQLite backup and loopback server. No emails or SMS are sent.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, fixture, request, client_hash


def run(args):
    target = fixture(args.port, source_db=args.source_db)
    database = target / 'db/main.db'
    # Exercise the first migration even when the source deployment has already
    # migrated. Reset only this disposable fixture; never touch the source DB.
    with sqlite3.connect(database) as db:
        for table in ('member_security_review', 'member_security_recovery', 'member_security_question'):
            db.execute(f'DROP TABLE IF EXISTS {table}')
        if db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name='xadmin_migration'").fetchone():
            db.execute("DELETE FROM xadmin_migration WHERE component='security'")
        db.commit()
    origin = f'http://127.0.0.1:{args.port}'
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': origin}))
    log = target / 'server.log'
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()

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
        result = json.loads(body)
        assert actual == status and result['code'] == (0 if status < 400 else status), (path, actual, result)
        return result['data'], response_headers

    def sql(statement, values=()):
        with sqlite3.connect(database) as db:
            cursor = db.execute(statement, values)
            rows = cursor.fetchall()
            db.commit()
            return rows

    def login(password=PASSWORD):
        data, headers = call('POST', '/api/v1/login', {'identifier': 'security_member', 'password': password})
        return data, headers['Set-Cookie'].split(';')[0]

    def admin_login(user=USER):
        actual, headers, body = request(args.port, 'POST', '/admin/login',
            {'username': user, 'password': client_hash(user, PASSWORD)})
        assert actual == 200 and json.loads(body)['result'], body
        return headers['Set-Cookie'].split(';')[0]

    answers = {'question1': 1, 'answer1': 'Private-Place-17', 'question2': 2, 'answer2': 'Private-Toy-29',
               'question3': 3, 'answer3': 'Private-School-31'}
    recovery_path = '/api/v1/auth/security-questions/recover'
    profile_path = '/api/v1/profile/security-questions'

    def submit(overrides=None):
        return call('POST', recovery_path, {'identifier': 'security_member', **answers, **(overrides or {})}, status=202)

    def ticket():
        return sql('SELECT id FROM member_security_recovery WHERE member_id=? AND closed_at=0', (owner,))[0][0]

    try:
        ready()
        assert len(call('GET', '/api/v1/auth/security-questions')[0]) == 8
        call('GET', profile_path, status=401)
        call('POST', '/api/v1/register', {'username': 'security_member', 'password': PASSWORD}, status=201)
        tokens, cookie = login()
        owner = tokens['id']
        _, other = login()
        admin = admin_login()
        admin_path = f'/admin/member/user/security?id={owner}'
        details, _ = call('GET', admin_path, cookie=admin)
        admin_csrf = {'X-CSRF-Token': details['csrf_token'], 'Origin': origin}
        assert not details['phone_verified'] and not details['email_verified']
        assert not details['security_questions_configured']
        call('PUT', profile_path, answers, cookie, status=403, headers={'X-CSRF-Token': ''})
        call('PUT', profile_path, answers, cookie, status=403, headers={'Origin': 'https://untrusted.example'})
        for malformed in ({'question2': 1}, {'question1': '1'}, {'question1': 0}, {'answer2': answers['answer1']},
                          {'answer1': 'abc'}, {'answer1': 'abc\u0000def'}, {'answer1': 'abc\ndef'}, {'enabled': True}):
            call('PUT', profile_path, {**answers, **malformed}, cookie, status=400)
        current, _ = call('GET', '/api/v1/session', cookie=cookie)
        sql('UPDATE member_session SET reauth_until=0 WHERE sid=?', (current['session_id'],))
        call('PUT', profile_path, answers, cookie, status=403)
        call('POST', '/api/v1/profile/reauth', {'password': PASSWORD}, cookie)
        call('PUT', profile_path, answers, cookie)
        call('GET', '/api/v1/profile', cookie=other, status=401)
        info, _ = call('GET', profile_path, cookie=cookie)
        assert info['configured'] and info['questions'] == [1, 2, 3] and info['recovery_mode'] == 'administrator_review'
        assert 'answer' not in json.dumps(info) and 'pbkdf2' not in json.dumps(info)
        records = sql('SELECT a1,a2,a3 FROM member_security_question WHERE member_id=?', (owner,))[0]
        assert all(value.startswith('pbkdf2-sha256$600000$') for value in records)
        assert all(answers[f'answer{i+1}'].lower() not in records[i] for i in range(3))
        assert len({value.split('$')[2] for value in records}) == 3
        # Simulate successful verification without sending any messages.
        sql("UPDATE member SET phone='+12025550177',phone_key='+12025550177',phone_verified_at=?,email='pending@example.com',email_key=NULL WHERE id=?", (int(time.time()*1e6), owner))
        profile, _ = call('GET', '/api/v1/profile', cookie=cookie)
        assert profile['phone_verified'] and not profile['email_verified'] and profile['security_questions_configured']
        for search in ('', '&search=%25security_member%25'):
            status, _, body = request(args.port, 'GET', '/admin/member/user?limit=100'+search, cookie=admin)
            assert status == 200
            item = next(item for item in json.loads(body)['data'] if item['id'] == owner)
            assert item['phone_verified'] and not item['email_verified'] and item['security_questions_configured']
        print('PASS security status on profile and admin lists; strict fields, recent proof, CSRF and salted answers')

        wrong, wrong_headers = submit({'answer1': 'Incorrect-answer'})
        absent, _ = submit({'identifier': 'absent_member'})
        correct, correct_headers = submit({'question1': 3, 'answer1': answers['answer3'],
                                          'question3': 1, 'answer3': '  PRIVATE-PLACE-17  '})
        assert wrong == absent == correct is None and 'Set-Cookie' not in correct_headers and 'Set-Cookie' not in wrong_headers
        assert sql('SELECT count(*) FROM member_security_recovery') == [(1,)]
        current_ticket = ticket()
        details, _ = call('GET', admin_path, cookie=admin)
        assert details['requests'][0]['id'] == current_ticket and 'pbkdf2' not in json.dumps(details)
        submit({'answer2': 'Wrong-answer-two'})
        submit({'question1': 4})
        submit({'answer3': 'Wrong-answer-three'})
        call('POST', recovery_path, {'identifier': 'security_member', **answers}, status=429)
        call('POST', recovery_path, {'identifier': 'security_member', **answers}, status=403, headers={'Origin': 'https://untrusted.example'})
        print('PASS uniform recovery responses, all answers required, no authentication, persistent target rate limits')

        reset = {'member_id': owner, 'request_id': current_ticket, 'action': 'reset_password',
                 'newPassword': 'Recovered-password-42', 'reason': 'Independent identity review completed', 'independently_verified': True}
        call('POST', admin_path, reset, admin, status=403)
        call('POST', admin_path, {**reset, 'independently_verified': False}, admin, status=400, headers=admin_csrf)
        call('POST', admin_path, {**reset, 'independently_verified': 'true'}, admin, status=400, headers=admin_csrf)
        denied = admin_login(USER+'_denied')
        assert request(args.port, 'GET', admin_path, cookie=denied)[0] == 403
        assert request(args.port, 'POST', admin_path, reset, cookie=denied, extra_headers=admin_csrf)[0] == 403
        assert request(args.port, 'GET', admin_path, extra_headers={'Authorization': 'Bearer '+tokens['access_token']})[0] == 302
        sql("CREATE TRIGGER fail_security_reset BEFORE UPDATE ON member BEGIN SELECT RAISE(ABORT,'test rollback');END")
        call('POST', admin_path, reset, admin, status=409, headers=admin_csrf)
        assert ticket() == current_ticket and not sql('SELECT id FROM member_security_review')
        call('GET', '/api/v1/profile', cookie=cookie)
        sql('DROP TRIGGER fail_security_reset')
        call('POST', admin_path, reset, admin, headers=admin_csrf)
        call('POST', admin_path, reset, admin, status=409, headers=admin_csrf)
        call('GET', '/api/v1/profile', cookie=cookie, status=401)
        call('GET', '/api/v1/profile', headers={'Authorization': 'Bearer '+tokens['access_token']}, status=401)
        call('POST', '/api/v1/token/refresh', {'refresh_token': tokens['refresh_token']}, status=401)
        call('POST', '/api/v1/login', {'identifier': 'security_member', 'password': PASSWORD}, status=401)
        tokens, cookie = login(reset['newPassword'])
        assert sql('SELECT action,admin_user,reason FROM member_security_review') == [('reset_password', USER, reset['reason'])]
        assert reset['newPassword'] not in str(sql('SELECT body FROM logs'))
        print('PASS admin RBAC/CSRF, explicit independent review, atomic rollback, one use and all-session revocation')

        sql('UPDATE identity_rate SET expires_at=0')
        submit(); stale = ticket()
        sql('UPDATE member_security_recovery SET expires_at=0 WHERE id=?', (stale,))
        call('POST', admin_path, {**reset, 'request_id': stale}, admin, status=409, headers=admin_csrf)
        sql('UPDATE identity_rate SET expires_at=0')
        submit(); stale = ticket()
        call('POST', '/api/v1/profile/password', {'oldPassword': reset['newPassword'], 'newPassword': PASSWORD}, cookie)
        call('POST', admin_path, {**reset, 'request_id': stale}, admin, status=409, headers=admin_csrf)
        sql('UPDATE identity_rate SET expires_at=0')
        submit(); stale = ticket()
        call('PUT', profile_path, {**answers, 'answer1': 'A-new-private-place'}, cookie)
        call('POST', admin_path, {**reset, 'request_id': stale}, admin, status=409, headers=admin_csrf)
        print('PASS expired requests and password/security question changes invalidate pending recovery')

        sql('UPDATE identity_rate SET expires_at=0')
        submit({'answer1': 'A-new-private-place'}); stale = ticket()
        actual, _, body = request(args.port, 'POST', '/admin/member/user/repwd',
            {'id': owner, 'username': 'security_member', 'password': PASSWORD}, admin)
        assert actual == 200 and json.loads(body)['result']
        call('POST', admin_path, {**reset, 'request_id': stale}, admin, status=409, headers=admin_csrf)
        call('GET', '/api/v1/profile', cookie=cookie, status=401)
        tokens, cookie = login()
        for field in ('phone_verified', 'email_verified', 'security_questions_configured'):
            assert request(args.port, 'PUT', '/admin/member/user', {'id': owner, field: True}, admin)[0] == 400
        print('PASS existing administrator password reset invalidates recovery; status fields are read only')

        call('POST', admin_path, {'member_id': owner, 'action': 'clear_questions', 'reason': 'Member requested verified removal'}, admin, headers=admin_csrf)
        call('GET', '/api/v1/profile', cookie=cookie, status=401)
        tokens, cookie = login()
        assert not call('GET', profile_path, cookie=cookie)[0]['configured']
        call('PUT', profile_path, answers, cookie)
        call('DELETE', profile_path, {}, cookie)
        assert not call('GET', '/api/v1/profile', cookie=cookie)[0]['security_questions_configured']
        # Feature can be disabled per site without deleting stored questions.
        call('PUT', profile_path, answers, cookie)
        (target / 'db/identity.json').write_text(json.dumps({'public_origin': origin, 'security_questions': False}))
        process.terminate(); process.wait(timeout=10); process = launch(); ready()
        assert not call('GET', '/api/v1/auth/providers')[0]['security_questions']
        assert call('GET', profile_path, cookie=cookie)[0]['configured']
        call('PUT', profile_path, answers, cookie, status=403)
        call('POST', recovery_path, {'identifier': 'security_member', **answers}, status=403)
        call('DELETE', profile_path, {}, cookie)
        assert sql("SELECT version FROM xadmin_migration WHERE component='security'") == [(1,)]
        assert (target / 'db/identity-before-security-v1.db').exists()
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original
        print('PASS member/admin removal, site feature switch, durable migration/restart; real user DB unchanged')
    except Exception:
        print(log.read_text(encoding='utf-8', errors='replace')[-4000:])
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
    parser.add_argument('--port', type=int, default=19261)
    parser.add_argument('--source-db', type=Path, help='consistent snapshot when the source is open on another OS')
    run(parser.parse_args())
