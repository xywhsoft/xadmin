"""Functional search proxy contracts against disposable xs/TCC fixtures; no load tests."""
import argparse
import hashlib
import json
import os
import http.client
from http.server import BaseHTTPRequestHandler
from pathlib import Path
import sqlite3
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request
from sms_unit import native_fixture


def run(args):
    target = fixture(args.port, source_db=args.source_db)
    origin = f'http://127.0.0.1:{args.port}'
    config = json.loads((target / 'xs.json').read_text())
    config['services'][0]['host_default']['devfile'] = str(ROOT / 'tests/search_host.c')
    (target / 'xs.json').write_text(json.dumps(config))
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': origin}))
    policy = json.loads((ROOT / 'plugin/web-search/config.defaults.json').read_text())
    policy.update(minute_limit=60, daily_limit=200, global_daily_limit=1000)
    (target / 'options/plugin').mkdir(exist_ok=True)
    (target / 'options/plugin/web-search.json').write_text(json.dumps(policy))
    log = target / 'server.log'
    environment = os.environ.copy()
    environment.pop('BOCHA_API_KEY', None); environment.pop('ZAI_API_KEY', None)

    def launch():
        with log.open('ab') as output:
            return subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                env=environment, stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    process = launch()
    tls_server = None
    success = False

    def ready():
        for _ in range(100):
            if process.poll() is not None: raise RuntimeError('xs exited')
            try:
                if request(args.port, 'GET', '/admin/login')[0] == 200: return
            except OSError: pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')

    def sql(statement, values=()):
        with sqlite3.connect(target / 'db/main.db') as db:
            rows = db.execute(statement, values).fetchall(); db.commit(); return rows

    def call(method, path, data=None, cookie=None, headers=None, status=200):
        actual, response_headers, raw = request(args.port, method, path, data, cookie, headers)
        assert actual == status, (method, path, actual, raw)
        assert b'test-bocha-secret' not in raw and b'test-zai-secret' not in raw, raw
        result = json.loads(raw)
        assert result['code'] == (0 if status < 400 else status), result
        return result.get('data'), response_headers

    def admin_login(user=USER):
        actual, headers, raw = request(args.port, 'POST', '/admin/login', {
            'username': user, 'password': client_hash(user, PASSWORD)})
        assert actual == 200 and json.loads(raw)['result'], raw
        return headers['Set-Cookie'].split(';')[0]

    def manage(action, cookie):
        actual, _, raw = request(args.port, 'POST', '/admin/plugin/' + action, {'name':'web-search'}, cookie)
        assert actual == 200, raw
        return json.loads(raw)

    def set_policy(patch):
        policy.update(patch)
        actual, _, raw = request(args.port, 'POST', '/admin/plugin/settings', {'name':'web-search', 'config':policy}, admin)
        assert actual == 200, (actual,raw)
        assert json.loads(raw)['result'], raw

    def stop():
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)

    try:
        ready(); admin = admin_login()
        assert manage('enable', admin)['result']
        denied = admin_login(USER + '_denied')
        assert request(args.port,'GET','/admin/web-search/credentials',cookie=denied)[0] == 403
        call('POST','/api/v1/search',{'query':'hello'},status=401)
        call('POST','/api/v1/register',{'username':'search_member','password':PASSWORD},status=201)
        tokens, response = call('POST','/api/v1/login',{'identifier':'search_member','password':PASSWORD})
        member = response['Set-Cookie'].split(';')[0]
        bearer = {'Authorization':'Bearer ' + tokens['access_token']}
        call('POST','/api/v1/search',{'query':'hello'},headers=bearer,status=403)
        sql("UPDATE member SET phone='+8613800138000',phone_key='+8613800138000',phone_verified_at=1 WHERE username='search_member'")
        call('POST','/api/v1/search',{'query':'hello'},headers=bearer,status=503)
        state, _ = call('GET','/admin/web-search/credentials',cookie=admin)
        assert not state['bocha']['configured'] and not state['zai']['configured']
        admin_headers = {'X-CSRF-Token':state['csrf_token'], 'Origin':origin}
        call('POST','/admin/web-search/credentials',{'bocha':'test-bocha-secret'},admin,status=403)
        call('POST','/admin/web-search/credentials',{'bocha':'test-bocha-secret'},admin,
             {'X-CSRF-Token':state['csrf_token'],'Origin':'https://other.example'},status=403)
        call('POST','/admin/web-search/credentials',{'bocha':'test-bocha-secret','zai':'test-zai-secret'},admin,admin_headers)
        call('GET','/admin/web-search/credentials',cookie=admin)
        _,_,public_config = request(args.port,'GET','/admin/plugin/get?name=web-search',cookie=admin)
        assert b'test-bocha-secret' not in public_config and b'test-zai-secret' not in public_config
        assert request(args.port,'GET','/plugin-static/web-search/../credentials.json')[0] == 404
        providers,_ = call('GET','/api/v1/search/providers',headers=bearer)
        assert [p['id'] for p in providers['providers']] == ['bocha','zai'] and all(p['available'] for p in providers['providers'])
        print('PASS plugin lifecycle, member/contact/admin authorization, CSRF and write-only credentials')

        for provider in ('bocha','zai'):
            data,_ = call('POST','/api/v1/search',{'query':'关键词','provider':provider,'count':1},headers=bearer)
            assert data['provider'] == provider and data['count'] == 1 and data['truncated']
            assert data['results'][0] == {'title':'测试标题','url':'https://example.com/1','snippet':'完整摘要','site':'Example','published_at':'2026-10-04'}
            data,_ = call('POST','/api/v1/search',{'query':'empty','provider':provider},headers=bearer)
            assert data['results'] == []
        call('POST','/api/v1/search',{'query':'freshness','freshness':'oneWeek'},headers=bearer)
        brief,_ = call('POST','/api/v1/search',{'query':'brief','summary':False},headers=bearer)
        assert brief['results'][0]['snippet'] == 'short'
        for patch in ({'query':''},{'query':' '},{'query':'a\u0000b'},{'query':'x'*1025},
                      {'query':'x','count':11},{'query':'x','count':True},{'query':'x','provider':'bad'},
                      {'query':'x','summary':'yes'},{'query':'x','endpoint':'https://evil.example'},
                      {'query':'x','provider':'zai','freshness':'oneDay'}):
            call('POST','/api/v1/search',patch,headers=bearer,status=400)
        call('GET','/api/v1/search',headers=bearer,status=405)
        call('POST','/api/v1/search',{'query':'hello'},member,{'X-CSRF-Token':''},status=403)
        call('POST','/api/v1/search',{'query':'hello'},member,{'X-CSRF-Token':tokens['csrf_token'],'Origin':origin})
        for query in ('upstream401','upstream429','redirect','malformed','application_error','transport'):
            call('POST','/api/v1/search',{'query':query},headers=bearer,status=502)
        call('POST','/api/v1/search',{'query':'x'},member,{'Authorization':'Bearer invalid'},status=401)
        call('POST','/admin/web-search/credentials',{'bocha':'bad\r\nkey'},admin,admin_headers,status=400)
        set_policy({'zai_enabled':False})
        call('POST','/api/v1/search',{'query':'x','provider':'zai'},headers=bearer,status=503)
        set_policy({'zai_enabled':True})
        set_policy({'zai_region':'cn','default_provider':'zai'})
        providers,_ = call('GET','/api/v1/search/providers',headers=bearer)
        assert providers['default_provider'] == 'zai'
        assert providers['providers'][1]['title'] == '智谱 GLM（国内）'
        data,_ = call('POST','/api/v1/search',{'query':'国内智谱','count':1},headers=bearer)
        assert data['provider'] == 'zai' and data['count'] == 1
        assert data['results'][0]['title'] == '测试标题'
        call('POST','/api/v1/search',{'query':'x','provider':'zai','freshness':'oneWeek'},headers=bearer,status=400)
        invalid = dict(policy,zai_region='https://evil.example')
        actual,_,raw = request(args.port,'POST','/admin/plugin/settings',{'name':'web-search','config':invalid},admin)
        assert actual == 200 and not json.loads(raw)['result'], raw
        # Old files lacking the region field continue to use the international API.
        policy.pop('zai_region')
        set_policy({'default_provider':'bocha'})
        data,_ = call('POST','/api/v1/search',{'query':'legacy region','provider':'zai','count':1},headers=bearer)
        assert data['count'] == 1
        print('PASS Bocha, z.ai and domestic GLM contracts, default selection, region validation and redacted failures')

        # One blocked request checks lock release and unload exclusion; not a load test.
        with ThreadPoolExecutor(max_workers=1) as executor:
            pending = executor.submit(call,'POST','/api/v1/search',{'query':'slow'},None,bearer)
            marker = target / 'temp/search-entered'
            for _ in range(50):
                if marker.exists(): break
                time.sleep(.02)
            else: raise AssertionError('transport not entered')
            call('GET','/api/v1/search/providers',headers=bearer)
            assert not pending.done(), 'request lock held during outbound call'
            assert not manage('disable',admin)['result'] and not manage('reload',admin)['result']
            call('POST','/api/v1/search',{'query':'hello'},headers=bearer,status=429)
            set_policy({'max_results':9})
            (target / 'temp/search-release').write_text('1')
            assert pending.result()[0]['count'] == 2
        assert manage('reload',admin)['result']
        usage,_ = call('GET','/api/v1/search/usage',headers=bearer)
        set_policy({'daily_limit':usage['daily_used']+1})
        call('POST','/api/v1/search',{'query':'hello'},headers=bearer)
        call('POST','/api/v1/search',{'query':'hello'},headers=bearer,status=429)
        print('PASS nonblocking outbound request, concurrent unload rejection, live config and persistent quota')

        stop(); process = launch(); ready(); admin = admin_login()
        # fixture database retains enabled plugin and budgets across process restart.
        call('POST','/api/v1/search',{'query':'hello'},headers=bearer,status=429)
        assert manage('disable',admin)['result']
        assert request(args.port,'GET','/api/v1/search/providers',extra_headers=bearer)[0] == 404
        assert manage('enable',admin)['result']
        set_policy({'daily_limit':200})
        usage,_ = call('GET','/api/v1/search/usage',headers=bearer)
        if usage['minute_used']:
            set_policy({'minute_limit':usage['minute_used']})
            call('POST','/api/v1/search',{'query':'x'},headers=bearer,status=429)
        set_policy({'minute_limit':60})
        with sqlite3.connect(target / 'db/plugin/web-search/plugin.db') as db:
            total = db.execute('SELECT used FROM search_budget WHERE owner=0 AND kind=1 AND window=?',(int(time.time())//86400,)).fetchone()[0]
        set_policy({'global_daily_limit':total})
        call('POST','/api/v1/search',{'query':'x'},headers=bearer,status=429)
        set_policy({'global_daily_limit':1000})
        sql("UPDATE member SET phone_verified_at=0,phone_key=NULL,phone='',email_verified_at=1,email_key='member@example.com',email='member@example.com' WHERE username='search_member'")
        call('GET','/api/v1/search/providers',headers=bearer,status=403)
        set_policy({'verification':'any'})
        call('GET','/api/v1/search/providers',headers=bearer)
        sql("UPDATE member SET email_verified_at=0,email_key=NULL,email='' WHERE username='search_member'")
        call('GET','/api/v1/search/providers',headers=bearer,status=403)
        set_policy({'verification':'none'})
        call('GET','/api/v1/search/providers',headers=bearer)
        credential_file = target / 'plugin_data/web-search/credentials.json'
        credential_file.write_text('{bad}')
        state,_ = call('GET','/admin/web-search/credentials',cookie=admin)
        assert state['file_invalid']
        admin_headers = {'X-CSRF-Token':state['csrf_token'],'Origin':origin}
        call('POST','/api/v1/search',{'query':'x'},headers=bearer,status=503)
        call('POST','/admin/web-search/credentials',{'bocha':'test-bocha-secret'},admin,admin_headers,status=409)
        call('POST','/admin/web-search/credentials',{'bocha':'test-bocha-secret','zai':'test-zai-secret'},admin,admin_headers)
        _,_,raw = request(args.port,'POST','/admin/plugin/settings',{'name':'web-search','config':dict(policy,timeout_ms=0)},admin)
        assert not json.loads(raw)['result']
        # A persisted invalid startup config fails closed rather than using zeros.
        assert manage('disable',admin)['result']
        policy_path = target / 'options/plugin/web-search.json'
        policy_path.write_text(json.dumps(dict(policy,verification='bad')))
        assert not manage('enable',admin)['result']
        policy_path.write_text(json.dumps(policy))
        assert manage('enable',admin)['result']
        print('PASS minute/global quotas, contact policies, credential repair and invalid-config rejection')
        environment['BOCHA_API_KEY'] = 'test-bocha-secret'
        stop(); process = launch(); ready(); admin = admin_login()
        state,_ = call('GET','/admin/web-search/credentials',cookie=admin)
        assert state['bocha']['environment'] and state['bocha']['configured']
        call('POST','/admin/web-search/credentials',{'bocha':''},admin,{'X-CSRF-Token':state['csrf_token'],'Origin':origin})
        call('POST','/api/v1/search',{'query':'hello'},headers=bearer)
        print('PASS process restart quota, re-enable and environment credential precedence')

        # Real HTTPS through async route + provider adapters, using a local CA.
        tls_server = native_fixture(target)
        native_captures = []
        class Handler(BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'
            def log_message(self,*args): pass
            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                bocha = self.path == '/bocha'
                assert self.headers['Authorization'] == 'Bearer ' + ('test-bocha-secret' if bocha else 'test-zai-secret')
                assert body['query' if bocha else 'search_query'] == 'native'
                native_captures.append(body)
                long_text = '完整摘要' * 10000
                raw = {'code':200,'data':{'webPages':{'value':[{'name':'TLS 搜索','url':'https://example.com/tls','summary':long_text}]}}} if bocha else {'search_result':[{'title':'TLS 搜索','link':'https://example.com/tls','content':long_text}]}
                payload = json.dumps(raw,ensure_ascii=False).encode()
                self.send_response(200); self.send_header('Content-Type','application/json')
                self.send_header('Content-Length',str(len(payload))); self.end_headers(); self.wfile.write(payload)
                self.close_connection = True
        tls_server.RequestHandlerClass = Handler
        native_path = target / 'temp/search-native-url'
        native_path.write_text(f'https://127.0.0.1:{tls_server.server_port}')
        for provider in ('bocha','zai'):
            data,response = call('POST','/api/v1/search',{'query':'native','provider':provider},headers=bearer)
            assert data['count'] == 1 and data['truncated'] and len(data['results'][0]['snippet'].encode()) <= 2048
            assert response['Connection'] == 'close'
        call('POST','/api/v1/search',{'query':'native-untrusted'},headers=bearer,status=502)
        native_path.unlink(); assert len(native_captures) == 2
        print('PASS actual HTTPS forwarding, 120 KiB upstream bodies, UTF-8 output bounds and untrusted CA rejection')

        # Peer disconnect keeps the plugin pinned until the owned callback ends.
        for name in ('search-entered','search-release'):
            (target / 'temp' / name).unlink(missing_ok=True)
        connection = http.client.HTTPConnection('127.0.0.1',args.port,timeout=5)
        connection.request('POST','/api/v1/search',body=json.dumps({'query':'slow'}),headers=dict(bearer,**{'Content-Type':'application/json'}))
        for _ in range(100):
            if (target / 'temp/search-entered').exists(): break
            time.sleep(.02)
        else: raise AssertionError('disconnected request did not start')
        connection.close()
        assert not manage('reload',admin)['result']
        (target / 'temp/search-release').write_text('1')
        for _ in range(50):
            if manage('reload',admin)['result']: break
            time.sleep(.02)
        else: raise AssertionError('plugin did not drain disconnected request')
        print('PASS peer disconnect cleanup and delayed plugin reload')

        for name in ('search-entered','search-release'):
            (target / 'temp' / name).unlink(missing_ok=True)
        with ThreadPoolExecutor(max_workers=1) as executor:
            pending = executor.submit(call,'POST','/api/v1/search',{'query':'slow'},None,bearer)
            for _ in range(100):
                if (target / 'temp/search-entered').exists(): break
                time.sleep(.02)
            else: raise AssertionError('reload request did not start')
            _,_,raw = request(args.port,'GET','/__test/search-state')
            old_marker = json.loads(raw)['marker']
            _,_,raw = request(args.port,'POST','/__test/search-state')
            assert json.loads(raw)['reload_id']
            for _ in range(100):
                _,_,raw = request(args.port,'GET','/__test/search-state')
                if json.loads(raw)['marker'] != old_marker: break
                time.sleep(.05)
            else: raise AssertionError('new host generation not published')
            (target / 'temp/search-release').write_text('1')
            assert pending.result()[0]['count'] == 2
        time.sleep(.1)
        admin = admin_login()
        call('GET','/admin/web-search/credentials',cookie=admin)
        call('POST','/api/v1/search',{'query':'after host reload'},headers=bearer)
        print('PASS full host reload while search is in flight and old-generation cleanup')
        logs = log.read_text(encoding='utf-8',errors='replace')
        assert 'test-bocha-secret' not in logs and 'test-zai-secret' not in logs
        assert '[tcc]' not in logs, logs
        success = True
    finally:
        stop()
        if tls_server: tls_server.shutdown(); tls_server.server_close()
        print('fixture:',target)
        if not success: print(log.read_text(encoding='utf-8',errors='replace')[-5000:])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,default=ROOT / ('xs.exe' if os.name=='nt' else 'xs'))
    parser.add_argument('--port',type=int,default=19295)
    parser.add_argument('--source-db',type=Path)
    run(parser.parse_args())
