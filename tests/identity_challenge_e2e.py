"""OTP/binding invariants using a test-only, in-process delivery adapter."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import time
from smoke import ROOT, PASSWORD, CSRF, fixture, request
from mfa_e2e import Client,totp

def run(args):
    original=hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest()
    target=fixture(args.port);database=target/'db/main.db'
    config=json.loads((target/'xs.json').read_text());config['services'][0]['host_default']['devfile']=str(target/'tests/identity_delivery_host.c')
    (target/'xs.json').write_text(json.dumps(config))
    (target/'db/identity.json').write_text(json.dumps({'public_origin':f'http://127.0.0.1:{args.port}','default_country_code':'+86'}))
    log=target/'server.log'
    with log.open('wb') as out:process=subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,stdout=out,stderr=subprocess.STDOUT,
        creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt'else 0)
    def call(method,path,data=None,cookie=None,status=200):
        actual,headers,body=request(args.port,method,path,data,cookie);value=json.loads(body)
        assert actual==status and value['code']==(0 if status<400 else status),(path,actual,value)
        payload=value.get('data')
        if payload and isinstance(payload,dict) and payload.get('csrf_token') and headers.get('Set-Cookie','').startswith('MSID='):
            CSRF[headers['Set-Cookie'].split(';')[0]]=payload['csrf_token']
        return payload,headers
    def sql(statement,values=()):
        with sqlite3.connect(database) as db:cursor=db.execute(statement,values);rows=cursor.fetchall();db.commit();return rows
    def start(channel,target_,cookie=None,purpose='login'):
        sql('UPDATE identity_rate SET expires_at=0') # deterministically advance only test rate buckets
        data,_=call('POST','/api/v1/profile/contacts/challenge'if cookie else'/api/v1/auth/challenges',
                    {'channel':channel,'target':target_,'purpose':purpose},cookie,status=202)
        delivered,_=call('GET','/__test/identity/delivery/'+data['challenge_id'])
        assert len(delivered['code'])==6
        return {'challenge_id':data['challenge_id'],'code':delivered['code']}
    try:
        for _ in range(80):
            if process.poll()is not None:raise RuntimeError('xs exited')
            try:
                if request(args.port,'GET','/admin/login')[0]==200:break
            except OSError:pass
            time.sleep(.2)
        else:raise RuntimeError('readiness timeout')
        for name in ('challenge_owner','challenge_other'):
            call('POST','/api/v1/register',{'username':name,'password':PASSWORD},status=201)
        auth,headers=call('POST','/api/v1/login',{'identifier':'challenge_owner','password':PASSWORD});cookie=headers['Set-Cookie'].split(';')[0]
        other_auth,headers=call('POST','/api/v1/login',{'identifier':'challenge_owner','password':PASSWORD});other=headers['Set-Cookie'].split(';')[0]
        challenge=start('phone','13800138000',cookie)
        call('POST','/api/v1/auth/challenges/verify',challenge,status=401)
        call('POST','/api/v1/profile/contacts/confirm',challenge,other,status=401)
        sql("CREATE TRIGGER fail_bind BEFORE UPDATE ON member BEGIN SELECT RAISE(ABORT,'test write failure');END")
        call('POST','/api/v1/profile/contacts/confirm',challenge,cookie,status=500)
        assert sql('SELECT consumed_at FROM identity_challenge WHERE id=?',(challenge['challenge_id'],))==[(0,)]
        assert sql("SELECT phone_verified_at FROM member WHERE username='challenge_owner'")==[(0,)]
        sql('DROP TRIGGER fail_bind')
        call('POST','/api/v1/profile/contacts/confirm',challenge,cookie)
        call('POST','/api/v1/profile/contacts/confirm',challenge,cookie,status=401)
        assert request(args.port,'GET','/api/v1/profile',cookie=other)[0]==401
        assert sql('SELECT code_hash FROM identity_challenge WHERE id=?',(challenge['challenge_id'],))[0][0]!=challenge['code']
        print('PASS purpose/session isolation, atomic binding rollback/retry, single consumption and other-session revocation')
        call('POST','/api/v1/login',{'identifier':'+8613800138000','password':PASSWORD})
        challenge=start('email','Alice@EXAMPLE.COM',cookie)
        wrong='000000'if challenge['code']!='000000'else'111111'
        for _ in range(5):call('POST','/api/v1/profile/contacts/confirm',{**challenge,'code':wrong},cookie,status=401)
        call('POST','/api/v1/profile/contacts/confirm',challenge,cookie,status=401)
        assert sql('SELECT attempts FROM identity_challenge WHERE id=?',(challenge['challenge_id'],))==[(5,)]
        challenge=start('email','Alice@EXAMPLE.COM',cookie)
        call('POST','/api/v1/profile/contacts/confirm',challenge,cookie)
        profile,_=call('GET','/api/v1/profile',cookie=cookie)
        assert profile['email']=='Alice@example.com'and profile['phone_verified']and profile['email_verified']
        # A second owner cannot take a verified target, and conflict leaves the
        # challenge usable if the original owner subsequently releases it.
        _,headers=call('POST','/api/v1/login',{'identifier':'challenge_other','password':PASSWORD});second=headers['Set-Cookie'].split(';')[0]
        challenge=start('phone','+8613800138000',second)
        call('POST','/api/v1/profile/contacts/confirm',challenge,second,status=409)
        assert sql('SELECT consumed_at FROM identity_challenge WHERE id=?',(challenge['challenge_id'],))==[(0,)]
        call('DELETE','/api/v1/profile/contacts',{'channel':'phone'},cookie)
        call('POST','/api/v1/profile/contacts/confirm',challenge,second)
        print('PASS five-attempt limit, mailbox canonicalization, verified uniqueness and contact removal')
        challenge=start('email','Alice@example.com',purpose='login')
        otp,headers=call('POST','/api/v1/auth/challenges/verify',challenge);otp_cookie=headers['Set-Cookie'].split(';')[0]
        call('GET','/api/v1/profile',cookie=otp_cookie)
        challenge=start('email','Alice@example.com',purpose='recover')
        call('POST','/api/v1/auth/challenges/verify',{**challenge,'newPassword':PASSWORD+'-recovered'})
        call('GET','/api/v1/profile',cookie=otp_cookie,status=401)
        call('GET','/api/v1/profile',cookie=cookie,status=401)
        call('POST','/api/v1/login',{'identifier':'challenge_owner','password':PASSWORD+'-recovered'})
        secured=Client(args.port)
        secured.api('POST','/api/v1/login',{'identifier':'challenge_owner','password':PASSWORD+'-recovered'})
        setup,_=secured.api('POST','/api/v1/profile/mfa/setup',{'password':PASSWORD+'-recovered'})
        enrolled,_=secured.api('POST','/api/v1/profile/mfa/confirm',{'setup_id':setup['setup_id'],'code':totp(setup['secret'])})
        challenge=start('email','Alice@example.com',purpose='login');candidate=Client(args.port)
        before=sql('SELECT count(*)FROM member_session')[0][0]
        pending,_=candidate.api('POST','/api/v1/auth/challenges/verify',challenge)
        assert pending['mfa_required']and 'access_token'not in pending and 'MSID'not in candidate.cookies
        assert sql('SELECT count(*)FROM member_session')[0][0]==before
        candidate.api('GET','/api/v1/profile',status=401)
        signed_in,_=candidate.api('POST','/api/v1/auth/mfa/verify',{'challenge_id':pending['challenge_id'],'code':enrolled['recovery_codes'][0]})
        old=candidate.cookies['MSID']
        challenge=start('email','Alice@example.com',purpose='recover')
        call('POST','/api/v1/auth/challenges/verify',{**challenge,'newPassword':PASSWORD+'-mfa-recovered'})
        assert sql("SELECT enabled FROM mfa_factor WHERE realm='member'AND owner=?",(signed_in['id'],))==[(1,)]
        Client(args.port).api('GET','/api/v1/profile',headers={'Cookie':'MSID='+old},status=401)
        after,_=Client(args.port).api('POST','/api/v1/login',{'identifier':'challenge_owner','password':PASSWORD+'-mfa-recovered'})
        assert after['mfa_required']and 'access_token'not in after
        print('PASS OTP primary login requires MFA and password recovery preserves the bound factor')
        # An account without a password must retain its final verified login.
        sql("INSERT INTO member(username,groupId,status,isDelete,email,email_key,email_verified_at)VALUES(NULL,1,1,0,'only@example.com','only@example.com',1)")
        challenge=start('email','only@example.com',purpose='login')
        _,headers=call('POST','/api/v1/auth/challenges/verify',challenge);only=headers['Set-Cookie'].split(';')[0]
        call('DELETE','/api/v1/profile/contacts',{'channel':'email'},only,status=409)
        # Capacity checks and resend cooldown are deterministic functional cases.
        sql('UPDATE identity_rate SET expires_at=0')
        call('POST','/api/v1/auth/challenges',{'purpose':'login','channel':'email','target':'cooldown@example.com'},status=202)
        call('POST','/api/v1/auth/challenges',{'purpose':'login','channel':'email','target':'cooldown@example.com'},status=429)
        assert hashlib.sha256((ROOT/'db/main.db').read_bytes()).digest()==original
        print('PASS OTP login, recovery revocation, final-login protection and resend cooldown; real DB unchanged')
    except Exception:
        print(log.read_text(encoding='utf-8',errors='replace')[-5000:]);raise
    finally:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        print('Fixture:',target)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=ROOT/('xs.exe'if os.name=='nt'else'xs'));p.add_argument('--port',type=int,default=19211);run(p.parse_args())
