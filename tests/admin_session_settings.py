"""Admin duration settings, upgrade compatibility and expiry in a disposable site."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from smoke import ROOT, USER, PASSWORD, DEFAULT_EXECUTABLE, client_hash, fixture, request

TIMEOUT = 'admin_session_timeout_minutes'
REMEMBER = 'admin_remember_days'


def run(args):
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    target = fixture(args.port, args.protected_entry)
    config_path = target / 'options/global.json'
    config = json.loads(config_path.read_text(encoding='utf-8'))
    # Simulate upgrading an installation whose global file predates these fields.
    for group in config['classList']:
        group['options'] = [f for f in group['options'] if f['name'] not in (TIMEOUT, REMEMBER)]
    config_path.write_text(json.dumps(config), encoding='utf-8')
    login_path = '/smoke-private-entry' if args.protected_entry else '/admin/login'
    log = target / 'server.log'
    process = None

    def stop():
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)

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
                if request(args.port, 'GET', login_path)[0] == 200:
                    return
            except OSError:
                pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')

    def login(remember=False):
        data = {'username': USER, 'password': client_hash(USER, PASSWORD)}
        if remember:
            data['remember'] = 'on'
        status, headers, body = request(args.port, 'POST', login_path, data)
        assert status == 200 and json.loads(body)['result'], 'admin login failed'
        return headers['Set-Cookie'].split(';')[0], headers['Set-Cookie']

    def probe(cookie, data=None):
        status, _, body = request(args.port, 'GET' if data is None else 'POST',
            '/__test/admin-session', data, cookie=cookie)
        assert status == 200, 'missing test session'
        return json.loads(body)

    def duration(cookie, seconds):
        state = probe(cookie)
        assert state['timeout'] == seconds and state['expires'] - state['active'] == seconds * 1000000, state
        return state

    def form(cookie, endpoint='/admin/option?file=global.json'):
        status, _, body = request(args.port, 'GET', endpoint, cookie=cookie)
        result = json.loads(body)
        assert status == 200 and result['result'], 'global form load failed'
        return result['data']

    def save(cookie, values, endpoint='/admin/option', success=True):
        status, _, body = request(args.port, 'POST', endpoint,
            {'file': 'global.json', 'source': 'option', 'data': values}, cookie=cookie)
        result = json.loads(body)
        assert status == 200 and result['result'] is success, result.get('message')
        if not success:
            assert '整数' in result['message'], result['message']

    def unauthorized(cookie):
        status, _, _ = request(args.port, 'GET', '/admin', cookie=cookie)
        assert status == (404 if args.protected_entry else 302), status

    try:
        launch()
        cookie, headers = login()
        assert 'Max-Age=' not in headers
        duration(cookie, 7200)
        disk_before = config_path.read_bytes()
        data = form(cookie)
        fields = {f['name']: f for g in data['schema']['groups'] for f in g['fields']}
        assert fields[TIMEOUT]['type'] == 'int' and fields[TIMEOUT]['props']['max'] == 43200
        assert fields[REMEMBER]['props']['min'] == 1 and fields[REMEMBER]['props']['max'] == 30
        values = data['values']
        assert values[TIMEOUT] == 120 and values[REMEMBER] == 7
        assert config_path.read_bytes() == disk_before, 'GET must not rewrite old configuration'
        remembered, headers = login(True)
        assert 'Max-Age=604800' in headers
        request(args.port, 'POST', '/admin/logout', cookie=remembered)
        # Older clients can submit partial data without losing the built-in defaults.
        save(cookie, {'cp_url': values['cp_url'], 'adminTitle': values['adminTitle']})
        persisted = json.loads(config_path.read_text(encoding='utf-8-sig'))
        stored = {f['name']: f['value'] for g in persisted['classList'] for f in g['options']}
        assert stored[TIMEOUT] == 120 and stored[REMEMBER] == 7
        print('PASS default durations and upgrade without replacing global configuration')

        disk_before = config_path.read_bytes()
        for endpoint in ('/admin/option', '/admin/form'):
            for name, maximum in ((TIMEOUT, 43200), (REMEMBER, 30)):
                for invalid in (0, -1, maximum + 1, 1.5, '5', '', None, True, [], {}):
                    bad = dict(values, **{name: invalid})
                    save(cookie, bad, endpoint, success=False)
                    assert config_path.read_bytes() == disk_before, 'invalid input changed persisted settings'
                    duration(cookie, 7200)
        print('PASS server validation on both configuration endpoints')

        values.update({TIMEOUT: 3, REMEMBER: 2})
        save(cookie, values, '/admin/form')
        duration(cookie, 180)
        remembered, headers = login(True)
        assert 'Max-Age=172800' in headers
        remembered_state = probe(remembered)
        assert remembered_state['remembered'] and remembered_state['expires'] - remembered_state['created'] == 172800 * 1000000
        transient, headers = login()
        assert 'Max-Age=' not in headers
        probe(remembered, {'idle_seconds': 120})
        idle_state = probe(transient, {'idle_seconds': 30})
        values[TIMEOUT] = 1
        save(cookie, values)
        state = duration(transient, 60)
        assert state['active'] == idle_state['active'], 'changing settings must not simulate activity'
        # Remembered sessions use an absolute deadline independent of the idle timeout.
        assert request(args.port, 'GET', '/admin', cookie=remembered)[0] == 200
        assert probe(remembered)['expires'] == remembered_state['expires'], 'remembered activity must not slide expiry'
        probe(remembered, {'idle_seconds': 172801, 'remember_elapsed': True})
        unauthorized(remembered)
        assert request(args.port, 'GET', '/admin', cookie=transient)[0] == 200
        state = duration(transient, 60)
        assert state['active'] > idle_state['active'], 'valid operations must renew the session'

        # Keep revoked and expired entries in the map to exercise refresh before pruning.
        probe(remembered := login()[0], {'expire': True})
        probe(transient, {'revoke': True})
        values[TIMEOUT] = 60
        save(cookie, values)
        assert probe(transient)['expires'] == -1
        assert probe(remembered)['expires'] < time.time_ns() // 1000
        unauthorized(transient)
        unauthorized(remembered)
        print('PASS hot configuration, renewal, immediate idle expiry and no session revival')

        values.update({TIMEOUT: 43200, REMEMBER: 30})
        save(cookie, values)
        duration(cookie, 2592000)
        remembered, headers = login(True)
        assert 'Max-Age=2592000' in headers
        state = probe(remembered)
        assert state['expires'] - state['created'] == 2592000 * 1000000
        assert form(cookie, '/admin/form?source=option&file=global.json')['values'][TIMEOUT] == 43200
        stop()
        launch()
        cookie, headers = login(True)
        duration(cookie, 2592000)
        assert 'Max-Age=2592000' in headers
        print('PASS maximum bounds, persistence and configuration restored on startup')

        stop()
        config = json.loads(config_path.read_text(encoding='utf-8-sig'))
        for group in config['classList']:
            for field in group['options']:
                if field['name'] == TIMEOUT:
                    field['value'] = -1
                if field['name'] == REMEMBER:
                    field['value'] = 'invalid'
        config_path.write_text(json.dumps(config), encoding='utf-8')
        disk_before = config_path.read_bytes()
        launch()
        cookie, headers = login(True)
        state = probe(cookie)
        assert state['expires'] - state['created'] == 604800 * 1000000
        assert 'Max-Age=604800' in headers
        values = form(cookie)['values']
        assert values[TIMEOUT] == 120 and values[REMEMBER] == 7
        assert config_path.read_bytes() == disk_before
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original
        print('PASS invalid disk settings use safe defaults; root database unchanged')
    finally:
        stop()
        print('Fixture:', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19181)
    parser.add_argument('--protected-entry', action='store_true')
    parser.add_argument('--exe', type=Path, default=DEFAULT_EXECUTABLE)
    run(parser.parse_args())
