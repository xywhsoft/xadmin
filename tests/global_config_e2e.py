"""Global policies and managed daily log cleanup against a disposable database."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time

from smoke import ROOT, USER, PASSWORD, DEFAULT_EXECUTABLE, client_hash, fixture, request

KEY = 'xadmin.admin.logs.cleanup'
FIELDS = {
    'admin_session_limit': (1, 100),
    'admin_login_failure_limit': (1, 100),
    'admin_login_cooldown_seconds': (1, 86400),
    'verification_code_ttl_seconds': (30, 3600),
    'verification_send_interval_seconds': (1, 3600),
    'verification_ip_limit_per_hour': (1, 10000),
    'admin_log_retention_days': (1, 3650),
    'admin_log_cleanup_hour': (0, 23),
}


def run(args):
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    target = fixture(args.port, args.protected_entry)
    database = target / 'db/main.db'
    config_path = target / 'options/global.json'
    config = json.loads(config_path.read_text(encoding='utf-8'))
    for group in config['classList']:
        group['options'] = [f for f in group['options'] if f['name'] not in FIELDS and f['name'] != 'admin_log_auto_cleanup']
    config_path.write_text(json.dumps(config), encoding='utf-8')
    service = json.loads((target / 'xs.json').read_text())
    service['services'][0]['host_default']['devfile'] = str(target / 'tests/identity_delivery_host.c')
    (target / 'xs.json').write_text(json.dumps(service))
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': f'http://127.0.0.1:{args.port}', 'default_country_code': '+86'}))
    login_path = '/smoke-private-entry' if args.protected_entry else '/admin/login'
    log = target / 'server.log'
    process = None

    def sql(statement, parameters=()):
        with sqlite3.connect(database) as db:
            cursor = db.execute(statement, parameters)
            rows = cursor.fetchall()
            db.commit()
            return rows

    def stop():
        if process and process.poll() is None:
            process.terminate()
            try: process.wait(timeout=10)
            except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)

    def launch():
        nonlocal process
        with log.open('ab') as output:
            process = subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError('xs exited: ' + log.read_text(encoding='utf-8', errors='replace')[-4000:])
            try:
                if request(args.port, 'GET', login_path)[0] == 200: return
            except OSError: pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')

    def login(password=PASSWORD):
        status, headers, body = request(args.port, 'POST', login_path,
            {'username': USER, 'password': client_hash(USER, password)})
        result = json.loads(body)
        return result, headers.get('Set-Cookie', '').split(';')[0]

    def call(method, path, data=None, cookie=None, success=True):
        status, _, body = request(args.port, method, path, data, cookie)
        result = json.loads(body)
        assert status == 200 and result['result'] is success, (path, status, result.get('message'))
        return result

    def save(values, endpoint='/admin/form', success=True):
        return call('POST', endpoint, {'source': 'option', 'file': 'global.json', 'data': values}, cookie, success)

    def wait_run(task_id, previous, status='success'):
        for _ in range(80):
            rows = sql('SELECT id,status,message FROM sched_run_log WHERE taskId=? AND id>? ORDER BY id DESC LIMIT 1', (task_id, previous))
            if rows and rows[0][1] == status and sql('SELECT runningCount FROM sched_task WHERE id=?', (task_id,)) == [(0,)]:
                return rows[0]
            time.sleep(.1)
        raise AssertionError(('task did not finish', rows))

    def last_run(task_id):
        return sql('SELECT COALESCE(MAX(id),0) FROM sched_run_log WHERE taskId=?', (task_id,))[0][0]

    def seed_logs():
        now = time.time_ns() // 1000
        sql("DELETE FROM logs WHERE uri='/__policy/log'")
        for days in (1, 4, 10):
            sql("INSERT INTO logs(user,ip,uri,method,param,body,createTime) VALUES(?,'','/__policy/log','GET','','',?)", (str(days), now - days * 86400 * 1000000))

    def retained():
        return sql("SELECT user FROM logs WHERE uri='/__policy/log' ORDER BY user")

    try:
        launch()
        result, cookie = login(); assert result['result']
        data = call('GET', '/admin/option?file=global.json', cookie=cookie)['data']
        values = data['values']
        assert set(FIELDS) <= set(values) and values['admin_log_auto_cleanup'] is False
        disk = config_path.read_bytes()
        for endpoint in ('/admin/form', '/admin/option'):
            for name, (lo, hi) in FIELDS.items():
                for bad in (lo - 1, hi + 1, '3', None, True, 1.5):
                    save(dict(values, **{name: bad}), endpoint, success=False)
                    assert config_path.read_bytes() == disk
            save(dict(values, admin_log_auto_cleanup='on'), endpoint, success=False)
        print('PASS legacy schema defaults and server validation for all new policies')

        values['admin_session_limit'] = 3; save(values)
        _, first = login(); _, second = login()
        time.sleep(.01)  # Windows host clock can have millisecond resolution.
        assert request(args.port, 'GET', '/admin', cookie=cookie)[0] == 200
        values['admin_session_limit'] = 2; save(values)
        assert request(args.port, 'GET', '/admin', cookie=first)[0] == (404 if args.protected_entry else 302)
        assert request(args.port, 'GET', '/admin', cookie=second)[0] == 200
        time.sleep(.01)
        assert request(args.port, 'GET', '/admin', cookie=cookie)[0] == 200
        _, third = login()
        assert request(args.port, 'GET', '/admin', cookie=second)[0] != 200
        cookie = third
        values.update(admin_session_limit=5, admin_login_failure_limit=2, admin_login_cooldown_seconds=1)
        save(values)
        assert not login('wrong')[0]['result']
        assert not login('wrong')[0]['result']
        assert '尝试过多' in login()[0]['message']
        time.sleep(1.2)
        result, cookie = login(); assert result['result'], result.get('message')
        print('PASS immediate account session cap, new-login eviction and configurable failure cooldown')

        values.update(verification_code_ttl_seconds=37, verification_send_interval_seconds=2, verification_ip_limit_per_hour=2)
        save(values)
        def challenge(target_, expected=202):
            status, _, raw = request(args.port, 'POST', '/api/v1/auth/challenges', {'purpose':'login','channel':'email','target':target_})
            result = json.loads(raw)
            assert status == expected, (status, result.get('message'))
            return result.get('data')
        challenge_data = challenge('policy@example.com')
        assert challenge_data['expires_in'] == 37
        stored = sql('SELECT expires_at-created_at FROM identity_challenge WHERE id=?', (challenge_data['challenge_id'],))
        assert stored == [(37,)]
        status, _, raw = request(args.port, 'GET', '/__test/identity/delivery/' + challenge_data['challenge_id'])
        assert status == 200 and json.loads(raw)['data']['expires_in'] == 37
        challenge('policy@example.com', 429)
        # The IP limit counts attempts, including an attempt rejected by target throttling.
        challenge('other@example.com', 429)
        values['verification_ip_limit_per_hour'] = 10; save(values)
        time.sleep(2.1)
        challenge('policy@example.com')
        print('PASS OTP expiry in storage, delivery and API; target interval and IP hourly limit')

        task_id = sql('SELECT id FROM sched_task WHERE systemKey=?', (KEY,))[0][0]
        assert sql('SELECT enabled FROM sched_task WHERE id=?', (task_id,)) == [(0,)]
        managed = call('GET', '/admin/sched/tasks', cookie=cookie)['data']
        assert any(row['id'] == task_id and row['managed'] for row in managed)
        for operation in ('delete', 'copy', 'enable'):
            call('POST', f'/admin/sched/task/{operation}?id={task_id}&enabled=1', cookie=cookie, success=False)
        call('PUT', '/admin/sched/task/save', {'id':task_id,'name':'override','execType':'shell','scheduleType':'once','onceAt':1,'codeText':'echo no'}, cookie, False)
        status, _, raw = request(args.port, 'GET', '/admin/sched/export', cookie=cookie)
        assert status == 200 and all(row['execType'] != 'builtin' for row in json.loads(raw))
        values.update(admin_log_retention_days=3, admin_log_cleanup_hour=0, admin_log_auto_cleanup=True)
        save(values)
        row = sql('SELECT enabled,cronExpr,misfirePolicy,overlapPolicy,nextRunAt FROM sched_task WHERE id=?', (task_id,))[0]
        assert row[:4] == (1, '0 0 0 * * * *', 'run_once', 'skip') and row[4] > time.time_ns() // 1000
        save(values)
        assert sql('SELECT id FROM sched_task WHERE systemKey=?', (KEY,)) == [(task_id,)]
        assert sql('SELECT nextRunAt FROM sched_task WHERE id=?', (task_id,)) == [(row[4],)]

        seed_logs(); previous = last_run(task_id)
        old_count = sql('SELECT count(*) FROM logs WHERE createTime < ?', (time.time_ns() // 1000 - 3 * 86400 * 1000000,))[0][0]
        sql('UPDATE sched_task SET nextRunAt=? WHERE id=?', (time.time_ns() // 1000 + 500000, task_id))
        save(values)  # Configuration save wakes the scheduler without replacing the due time.
        run = wait_run(task_id, previous)
        assert f'共 {old_count} 条' in run[2] and retained() == [('1',)]
        print('PASS unique managed daily task, protected lifecycle and automatic deletion at configured retention')

        seed_logs(); previous = last_run(task_id)
        sql("CREATE TRIGGER fail_policy_cleanup BEFORE DELETE ON logs BEGIN SELECT RAISE(ABORT,'cleanup test failure'); END")
        call('POST', f'/admin/sched/task/run?id={task_id}', cookie=cookie)
        wait_run(task_id, previous, 'failed')
        assert len(retained()) == 3
        assert sql('SELECT lastStatus,retryState FROM sched_task WHERE id=?', (task_id,)) == [('retrying', 1)]
        sql('DROP TRIGGER fail_policy_cleanup')
        previous = last_run(task_id)
        call('POST', f'/admin/sched/task/run?id={task_id}', cookie=cookie)
        wait_run(task_id, previous)
        assert retained() == [('1',)]

        seed_logs(); previous = last_run(task_id)
        stop()
        sql('UPDATE sched_task SET nextRunAt=?,isRunning=1,runningCount=1 WHERE id=?', (time.time_ns() // 1000 - 1000000, task_id))
        launch(); wait_run(task_id, previous)
        assert retained() == [('1',)]
        assert sql('SELECT id FROM sched_task WHERE systemKey=?', (KEY,)) == [(task_id,)]
        result, cookie = login(); assert result['result']
        values['admin_log_auto_cleanup'] = False; save(values)
        assert sql('SELECT enabled,nextRunAt FROM sched_task WHERE id=?', (task_id,)) == [(0, 0)]
        seed_logs(); previous = last_run(task_id)
        call('POST', f'/admin/sched/task/run?id={task_id}', cookie=cookie)
        wait_run(task_id, previous)
        assert len(retained()) == 3
        cleared = call('POST', '/admin/logs/clear', cookie=cookie)
        assert cleared['retentionDays'] == 3 and retained() == [('1',)]
        print('PASS cleanup failure rollback/retry, restart catch-up, disable and manual cleanup linkage')

        sql("CREATE TRIGGER fail_policy_sync BEFORE UPDATE ON sched_task WHEN OLD.systemKey='xadmin.admin.logs.cleanup' BEGIN SELECT RAISE(ABORT,'sync test failure'); END")
        assert '同步失败' in save(values, success=False)['message']
        sql('DROP TRIGGER fail_policy_sync'); save(values)
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original
        print('PASS task synchronization failure is reported; root database unchanged')
    finally:
        stop()
        print('Fixture:', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19189)
    parser.add_argument('--protected-entry', action='store_true')
    parser.add_argument('--exe', type=Path, default=DEFAULT_EXECUTABLE)
    run(parser.parse_args())
