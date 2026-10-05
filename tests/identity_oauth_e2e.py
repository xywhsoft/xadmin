"""Browser-bound GitHub/WeChat flows with SDK transport injection, no Internet."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from urllib.parse import parse_qs,urlparse,urlencode
from smoke import ROOT,PASSWORD,CSRF,fixture,request
from mfa_e2e import Client,totp

def run(args):
    original=hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest();target=fixture(args.port);database=target/'db/main.db'
    config=json.loads((target/'xs.json').read_text());config['services'][0]['host_default']['devfile']=str(target/'tests/identity_oauth_host.c');(target/'xs.json').write_text(json.dumps(config))
    origin=f'http://127.0.0.1:{args.port}'
    (target/'db/identity.json').write_text(json.dumps({'public_origin':origin,
        'github':{'enabled':True,'client_id':'github-test','client_secret':'github-secret','callback':origin+'/api/v1/auth/oauth/github/callback'},
        'wechat':{'enabled':True,'client_id':'wechat-test','client_secret':'wechat-secret','callback':origin+'/api/v1/auth/oauth/wechat/callback'}}))
    log=target/'server.log'
    def launch():
        with log.open('ab')as out:return subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,stdout=out,stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt'else 0)
    process=launch()
    def ready():
        for _ in range(80):
            if process.poll()is not None:raise RuntimeError('xs exited')
            try:
                if request(args.port,'GET','/admin/login')[0]==200:return
            except OSError:pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')
    def call(method,path,data=None,cookie=None,status=200):
        actual,headers,body=request(args.port,method,path,data,cookie)
        assert actual==status,(path,actual,body)
        result=json.loads(body)if body else None
        if result:assert result['code']==(0 if status<400 else status),(path,result)
        return result['data']if result else None,headers
    def sql(statement,values=()):
        with sqlite3.connect(database)as db:cursor=db.execute(statement,values);rows=cursor.fetchall();db.commit();return rows
    def start(provider,cookie=None,action='start'):
        prefix='/api/v1/auth/oauth/'if action=='start'else'/api/v1/profile/identities/'
        data,headers=call('POST',prefix+provider+'/'+action,{},cookie)
        query=parse_qs(urlparse(data['authorization_url']).query)
        assert query['client_id'if provider=='github'else'appid']==[provider+'-test']
        if provider=='github':assert query['code_challenge_method']==['S256']and query['code_challenge'][0]
        else:assert query['scope']==['snsapi_login']
        browser=headers['Set-Cookie'].split(';')[0]
        return query['state'][0],browser
    def callback(provider,state,browser,code='gh_one',status=303):
        return call('GET','/api/v1/auth/oauth/'+provider+'/callback?'+urlencode({'state':state,'code':code}),cookie=browser,status=status)
    try:
        ready();providers,_=call('GET','/api/v1/auth/providers');assert [p['id']for p in providers['providers']]==['github','wechat']
        assert 'secret'not in json.dumps(providers)
        error_status,error_headers,error_body=request(args.port,'GET','/api/v1/auth/oauth/github/callback?state=invalid&code=untrusted',extra_headers={'Accept':'text/html'})
        assert error_status==401 and error_headers['Content-Type'].startswith('text/html')
        assert b'/account/index.html' in error_body and b'untrusted' not in error_body
        call('POST','/api/v1/register',{'username':'local_member','password':PASSWORD},status=201)
        sql("UPDATE member SET email='same@example.com' WHERE username='local_member'")
        state,browser=start('github')
        callback('github',state,'MOB='+'a'*64,status=401)
        callback('wechat',state,browser,status=401)
        _,headers=callback('github',state,browser);cookie=headers['Set-Cookie'].split(';')[0]
        call('GET','/api/v1/profile',cookie=cookie)
        profile,_=call('GET','/api/v1/profile',cookie=cookie);owner=profile['id'];assert profile['username']==''and not profile['email_verified']and profile['email']==''
        callback('github',state,browser,status=401)
        assert sql("SELECT member_id FROM member_external_identity WHERE provider='github'AND subject='101'")==[(owner,)]
        assert sql("SELECT id FROM member WHERE username='local_member'")[0][0]!=owner
        state,browser=start('github');_,headers=callback('github',state,browser,code='renamed')
        profile,_=call('GET','/api/v1/profile',cookie=headers['Set-Cookie'].split(';')[0]);assert profile['id']==owner
        print('PASS GitHub PKCE, immutable numeric subject, browser/provider binding, single callback and no email merge')
        # Binding a second external provider to the same member is explicit.
        cookie=headers['Set-Cookie'].split(';')[0]
        state,browser=start('wechat',cookie,action='bind');callback('wechat',state,browser+'; '+cookie,code='wx_one')
        identities,_=call('GET','/api/v1/profile/identities',cookie=cookie);assert len(identities)==2
        assert {i['provider']for i in identities}=={'github','wechat'}
        assert sql("SELECT subject,app_namespace,union_id,union_scope FROM member_external_identity WHERE provider='wechat'")==[('wx_openid_1','wechat-test','wx_union_1',None)]
        state,browser=start('wechat');_,headers=callback('wechat',state,browser,code='wx_one')
        wx_cookie=headers['Set-Cookie'].split(';')[0];profile,_=call('GET','/api/v1/profile',cookie=wx_cookie);assert profile['id']==owner
        state,browser=start('wechat');callback('wechat',state,browser,code='bad_openid',status=502)
        _,headers=call('POST','/api/v1/login',{'identifier':'local_member','password':PASSWORD});local_cookie=headers['Set-Cookie'].split(';')[0]
        state,browser=start('github',local_cookie,action='bind');callback('github',state,browser+'; '+local_cookie,status=409)
        print('PASS WeChat token/userinfo identity agreement, AppID namespace, explicit bind and ownership conflict')
        # Passwordless OAuth accounts can enroll using their recent primary proof.
        mfa=Client(args.port);mfa.cookies['MSID']=wx_cookie.split('=',1)[1]
        active,_=mfa.api('GET','/api/v1/session');mfa.csrf=active['csrf_token']
        setup,_=mfa.api('POST','/api/v1/profile/mfa/setup',{})
        enrolled,_=mfa.api('POST','/api/v1/profile/mfa/confirm',{'setup_id':setup['setup_id'],'code':totp(setup['secret'])})
        recovery_codes=enrolled['recovery_codes']
        for index,provider in enumerate(('github','wechat')):
            state,browser=start(provider);candidate=Client(args.port)
            candidate.cookies['MOB']=browser.split('=',1)[1]
            before=sql('SELECT count(*)FROM member_session')[0][0]
            actual,response,body=candidate.raw('GET','/api/v1/auth/oauth/'+provider+'/callback?'+urlencode({'state':state,'code':'gh_one'if provider=='github'else'wx_one'}),headers={'Accept':'text/html'})
            assert actual==303 and not body and 'MMFA'in candidate.cookies and 'MSID'not in candidate.cookies
            assert sql('SELECT count(*)FROM member_session')[0][0]==before
            candidate.api('GET','/api/v1/profile',status=401)
            pending,_=candidate.api('GET','/api/v1/auth/mfa/pending')
            signed_in,_=candidate.api('POST','/api/v1/auth/mfa/verify',{'challenge_id':pending['challenge_id'],'code':recovery_codes[index]})
            assert signed_in['id']==owner and 'MMFA'not in candidate.cookies
            wx_cookie='MSID='+candidate.cookies['MSID'];CSRF[wx_cookie]=signed_in['csrf_token']
        print('PASS GitHub/WeChat primary login cannot issue a session before MFA; passwordless enrollment and browser handoff')
        # Remove one method, but never the final usable login method.
        identities,_=call('GET','/api/v1/profile/identities',cookie=wx_cookie)
        call('DELETE','/api/v1/profile/identities/'+str(identities[0]['id']),cookie=wx_cookie)
        call('DELETE','/api/v1/profile/identities/'+str(identities[1]['id']),cookie=wx_cookie,status=409)
        # Disabled accounts remain reserved rather than recreated by SSO.
        before=sql('SELECT count(*) FROM member')[0][0];sql('UPDATE member SET status=0 WHERE id=?',(owner,))
        state,browser=start('wechat');callback('wechat',state,browser,code='wx_one',status=403)
        assert sql('SELECT count(*) FROM member')[0][0]==before
        sql('UPDATE member SET status=1 WHERE id=?',(owner,))
        state,browser=start('github');call('GET','/api/v1/auth/oauth/github/callback?'+urlencode({'state':state,'error':'access_denied'}),cookie=browser,status=401)
        callback('github',state,browser,status=401)
        state,browser=start('github');callback('github',state,browser,code='denied',status=502);callback('github',state,browser,status=401)
        print('PASS unbind ownership/final-login checks, disabled account preservation, cancellation and failed exchange consumption')
        state,browser=start('github')
        sql('UPDATE identity_oauth SET expires_at=0 WHERE state_hash=?',(hashlib.sha256(state.encode()).hexdigest(),))
        callback('github',state,browser,status=401)
        sql('UPDATE identity_rate SET expires_at=0')
        state,browser=start('github');process.terminate();process.wait(timeout=10);process=launch();ready()
        callback('github',state,browser,status=401)
        sql('UPDATE identity_rate SET expires_at=0')
        state,browser=start('github')
        # Two ordinary user requests prove network work releases the application
        # mutex; this is a functional lifecycle check, not a load test.
        with ThreadPoolExecutor(max_workers=1)as pool:
            pending=pool.submit(callback,'github',state,browser,'delayed')
            time.sleep(.08);assert call('GET','/api/v1/auth/providers')[0]['providers'];pending.result()
        assert hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest()==original
        assert sql("SELECT count(*)FROM identity_oauth WHERE state_hash=?AND status=2",(hashlib.sha256(state.encode()).hexdigest(),))==[(1,)]
        print('PASS expiration/restart cancellation, network lock release and consumption ledger; real DB unchanged')
    except Exception:
        print(log.read_text(encoding='utf-8',errors='replace')[-5000:]);raise
    finally:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        print('Fixture:',target)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=ROOT/('xs.exe'if os.name=='nt'else'xs'));p.add_argument('--port',type=int,default=19221);run(p.parse_args())
