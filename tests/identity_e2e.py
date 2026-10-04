"""Functional identity contracts; disposable DB, no real provider/SMS traffic."""
import argparse
import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from smoke import ROOT, PASSWORD, fixture, request, client_hash

def b64(data):
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode()

def run(args):
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    target = fixture(args.port)
    config=json.loads((target/'xs.json').read_text())
    config['services'][0]['host_default']['devfile']=str(ROOT/'main.c')
    (target/'xs.json').write_text(json.dumps(config))
    (target/'db/identity.json').write_text(json.dumps({
        'public_origin': f'http://127.0.0.1:{args.port}', 'default_country_code': '+86'}))
    db_path = target/'db/main.db'
    with sqlite3.connect(db_path) as db:
        name, salt = 'legacy_identity', 'old-salt'
        stored = hashlib.sha256((name+salt+client_hash(name,PASSWORD)).encode()).hexdigest().upper()
        db.execute("INSERT INTO member(username,salt,pwd,groupId,status,isDelete,phone,email)VALUES(?,?,?,1,1,0,?,?)",
                   (name,salt,stored,'13800138000','old@example.com'))
        db.commit()
    log = target/'server.log'
    with log.open('wb') as out:
        process = subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,
                                   stdout=out,stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    def call(method,path,data=None,cookie=None,headers=None,status=200):
        actual, response_headers, body = request(args.port,method,path,data,cookie,headers)
        result = json.loads(body)
        assert actual==status and result['code']==(0 if status<400 else status),(path,actual,result)
        return result.get('data'),response_headers
    def sql(statement,values=()):
        with sqlite3.connect(db_path) as db:
            cursor=db.execute(statement,values);rows=cursor.fetchall();db.commit();return rows
    try:
        for _ in range(80):
            if process.poll() is not None: raise RuntimeError('xs exited')
            try:
                if request(args.port,'GET','/admin/login')[0]==200: break
            except OSError: pass
            time.sleep(.2)
        else: raise RuntimeError('readiness timeout')
        data, headers=call('POST','/api/v1/login',{'identifier':'LEGACY_IDENTITY','password':PASSWORD})
        cookie=headers['Set-Cookie'].split(';')[0]
        token=data['access_token'];refresh=data['refresh_token'];csrf=data['csrf_token']
        bearer={'Authorization':'Bearer '+token}
        assert len(cookie.split('=',1)[1])==64
        password=sql('SELECT pwd,salt FROM member WHERE username=?',('legacy_identity',))[0]
        assert password[0].startswith('pbkdf2-sha256$600000$') and password[1]==''
        assert sql('SELECT cookie_hash FROM member_session')[0][0]!=cookie.split('=',1)[1]
        assert sql('SELECT hash FROM member_refresh')[0][0]!=refresh
        call('GET','/api/v1/profile',headers=bearer)
        call('GET','/api/v1/profile',cookie=cookie,headers={'Authorization':'Bearer invalid'},status=401)
        assert request(args.port,'GET','/admin',extra_headers=bearer)[0]==302
        call('PUT','/api/v1/profile',{'nickname':'changed'},cookie,{'X-CSRF-Token':''},status=403)
        call('PUT','/api/v1/profile',{'nickname':'changed'},cookie,{'Origin':'https://other.example'},status=403)
        call('PUT','/api/v1/profile',{'nickname':'changed'},cookie)
        for key in ('phone','email','phone_verified_at','email_verified_at','groupId','status'):
            call('PUT','/api/v1/profile',{key:''},cookie,status=400)
        call('POST','/api/v1/login',{'identifier':'old@example.com','password':PASSWORD},status=401)
        call('POST','/api/v1/login',{'identifier':'+8613800138000','password':PASSWORD},status=401)
        # Deliberate fixtures prove verified identifiers use the same password
        # credential without changing email local-part or country rules.
        sql("UPDATE member SET email='Alice@example.com',email_key='Alice@example.com',email_verified_at=1,phone='+8613800138000',phone_key='+8613800138000',phone_verified_at=1 WHERE username='legacy_identity'")
        call('POST','/api/v1/login',{'identifier':'Alice@EXAMPLE.COM','password':PASSWORD})
        call('POST','/api/v1/login',{'identifier':'alice@example.com','password':PASSWORD},status=401)
        call('POST','/api/v1/login',{'identifier':'13800138000','password':PASSWORD})
        print('PASS raw-password legacy upgrade, verified identifiers, Cookie/Bearer separation and CSRF')
        # Re-sign malformed claims with the real disposable key: prove required
        # claims and algorithm pinning independently of signature corruption.
        claims=json.loads(base64.urlsafe_b64decode(token.split('.')[1]+'=='))
        header=json.loads(base64.urlsafe_b64decode(token.split('.')[0]+'=='))
        secret=sql('SELECT secret FROM identity_key WHERE active=1')[0][0]
        def signed(head,payload):
            content=b64(json.dumps(head).encode())+'.'+b64(json.dumps(payload).encode())
            return content+'.'+b64(hmac.new(secret.encode(),content.encode(),hashlib.sha256).digest())
        for field in ('exp','iat','iss','aud','sub','jti','sid','realm'):
            malformed=dict(claims);malformed.pop(field)
            call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+signed(header,malformed)},status=401)
        for replacement in ({'exp':int(time.time())},{'iat':int(time.time())+60},{'realm':'admin'}, {'sub':'01'}, {'aud':'admin'}):
            call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+signed(header,{**claims,**replacement})},status=401)
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+signed({**header,'alg':'HS384'},claims)},status=401)
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+signed({**header,'jku':'https://other.example'},claims)},status=401)
        print('PASS pinned JWT algorithm, required claims, expiry, realm and subject checks')
        rotated,_=call('POST','/api/v1/token/refresh',{'refresh_token':refresh})
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+rotated['access_token']})
        call('POST','/api/v1/token/refresh',{'refresh_token':refresh},status=401)
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+rotated['access_token']},status=401)
        call('GET','/api/v1/profile',cookie=cookie,status=401)
        print('PASS refresh rotation and replay revokes the entire session family')
        active,_=call('POST','/api/v1/login',{'identifier':'legacy_identity','password':PASSWORD})
        active_claims=json.loads(base64.urlsafe_b64decode(active['access_token'].split('.')[1]+'=='))
        active_sid=active_claims['sid'];now=int(time.time())
        sql('UPDATE member_session SET created_at=?,last_used=?,expires_at=? WHERE sid=?',
            (now-45*86400,now-120,now+600,active_sid))
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+active['access_token']})
        expiry,last_used=sql('SELECT expires_at,last_used FROM member_session WHERE sid=?',(active_sid,))[0]
        assert abs(expiry-(now+30*86400))<10 and last_used>=now
        # The same minute does not write the session again, and bad JWTs cannot renew it.
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+active['access_token']})
        assert sql('SELECT expires_at,last_used FROM member_session WHERE sid=?',(active_sid,))[0]==(expiry,last_used)
        active,_=call('POST','/api/v1/token/refresh',{'refresh_token':active['refresh_token']})
        sql('UPDATE member_session SET expires_at=? WHERE sid=?',(now-1,active_sid))
        call('GET','/api/v1/profile',headers={'Authorization':'Bearer '+active['access_token']},status=401)
        call('POST','/api/v1/token/refresh',{'refresh_token':active['refresh_token']},status=401)
        assert sql('SELECT expires_at FROM member_session WHERE sid=?',(active_sid,))==[(now-1,)]
        print('PASS active sliding renewal beyond 30 days, bounded writes, and expired sessions cannot revive')
        data,headers=call('POST','/api/v1/login',{'identifier':'legacy_identity','password':PASSWORD})
        cookie=headers['Set-Cookie'].split(';')[0];bearer={'Authorization':'Bearer '+data['access_token']}
        call('GET','/api/v1/sessions',cookie=cookie)
        # A real restart, not just a value-map reset.
        process.terminate();process.wait(timeout=10)
        with log.open('ab') as out:
            process=subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,
                stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        for _ in range(80):
            try:
                if request(args.port,'GET','/admin/login')[0]==200: break
            except OSError: pass
            time.sleep(.2)
        call('GET','/api/v1/profile',headers=bearer)
        call('GET','/api/v1/profile',cookie=cookie)
        # An optional absolute cap applies after restart without reviving expired sessions.
        policy=json.loads((target/'db/identity.json').read_text());policy['session_max_days']=1
        (target/'db/identity.json').write_text(json.dumps(policy))
        cap_claims=json.loads(base64.urlsafe_b64decode(data['access_token'].split('.')[1]+'=='))
        sql('UPDATE member_session SET created_at=? WHERE sid=?',(int(time.time())-2*86400,cap_claims['sid']))
        process.terminate();process.wait(timeout=10)
        with log.open('ab') as out:
            process=subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,
                stdout=out,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        for _ in range(80):
            try:
                if request(args.port,'GET','/admin/login')[0]==200: break
            except OSError: pass
            time.sleep(.2)
        call('GET','/api/v1/profile',headers=bearer,status=401)
        call('POST','/api/v1/token/refresh',{'refresh_token':data['refresh_token']},status=401)
        sql('UPDATE member_session SET created_at=? WHERE sid=?',(int(time.time()),cap_claims['sid']))
        print('PASS configurable absolute lifetime still requires reauthentication')
        call('POST','/api/v1/logout',{},cookie)
        call('GET','/api/v1/profile',headers=bearer,status=401)
        call('POST','/api/v1/token/refresh',{'refresh_token':data['refresh_token']},status=401)
        assert hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest()==original
        print('PASS sessions survive restart; logout revocation persists; real user DB unchanged')
    finally:
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill();process.wait(timeout=10)
        print('Fixture:',target)
        if process.returncode not in (0,1,-15):print(log.read_text(encoding='utf-8',errors='replace')[-4000:])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=ROOT/('xs.exe'if os.name=='nt'else'xs'))
    p.add_argument('--port',type=int,default=19201);run(p.parse_args())
