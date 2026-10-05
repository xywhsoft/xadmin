"""Transactional billing, membership and legacy cutover; disposable database."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--exe',type=Path,default=ROOT/'xs.exe')
    p.add_argument('--port',type=int,default=19185)
    args=p.parse_args();target=fixture(args.port)
    shutil.copytree(ROOT/'tests/plugins/billing-sdk',target/'plugin/billing-sdk')
    config=json.loads((target/'xs.json').read_text());config['services'][0]['host_default']['devfile']=str(target/'main.c')
    (target/'xs.json').write_text(json.dumps(config))
    origin=f'http://127.0.0.1:{args.port}'
    (target/'db/identity.json').write_text(json.dumps({'public_origin':origin}))
    log=target/'billing.log'
    def launch():
        with log.open('ab') as output:
            return subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,
                stdout=output,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    process=launch()
    def ready():
        for _ in range(100):
            if process.poll() is not None: raise RuntimeError('xs exited')
            try:
                if request(args.port,'GET','/admin/login')[0]==200:return
            except OSError:pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')
    def call(method,path,body=None,cookie=None,headers=None,status=200):
        actual,h,raw=request(args.port,method,path,body,cookie,headers)
        assert actual==status,(path,actual,raw)
        result=json.loads(raw);return result.get('data'),h,result
    def login():
        _,h,r=call('POST','/admin/login',{'username':USER,'password':client_hash(USER,PASSWORD)})
        assert r['result'];return h['Set-Cookie'].split(';')[0]
    def manage(action,name):
        _,_,r=call('POST','/admin/plugin/'+action,{'name':name},admin);assert r['result'],r
    def sql(path,query,params=()):
        with sqlite3.connect(path) as db:rows=db.execute(query,params).fetchall();db.commit();return rows
    def mutate(path,body,status=200):return call('POST',path,body,admin,admin_headers,status)
    def account():return call('GET','/api/v1/billing/account',headers=bearer)[0]
    def operation(action,id,amount=0,outcome='settled',status=200):
        call('POST','/__test/billing',{'action':action,'request_id':id,'amount':amount,'outcome':outcome},headers=bearer,status=status)
    try:
        ready();admin=login()
        member,_,_=call('POST','/api/v1/register',{'username':'billing_member','password':PASSWORD},status=201);owner=member['id']
        sql(target/'db/main.db','UPDATE member SET balance=123 WHERE id=?',(owner,))
        tokens,_,_=call('POST','/api/v1/login',{'identifier':'billing_member','password':PASSWORD})
        bearer={'Authorization':'Bearer '+tokens['access_token']}
        manage('enable','billing');manage('enable','billing-sdk')
        state,_,_=call('GET','/admin/billing/state',cookie=admin)
        admin_headers={'X-CSRF-Token':state['csrf_token'],'Origin':origin}
        db=target/'db/plugin/billing/plugin.db'
        assert account()['cash_micros']==1230000
        adjust={'member_id':owner,'amount_micros':1000000,'operation_id':'topup.1','reason':'Test topup'}
        mutate('/admin/billing/adjust',adjust,status=200);mutate('/admin/billing/adjust',adjust)
        mutate('/admin/billing/adjust',dict(adjust,amount_micros=1000001),status=409)
        assert account()['cash_micros']==2230000
        assert call('GET','/api/v1/balance',headers=bearer)[0]['balance_micros']==2230000
        assert sql(target/'db/main.db','SELECT balance FROM member WHERE id=?',(owner,))[0][0]==123
        operation('reserve','request.1',1000000)
        assert account()['cash_reserved_micros']==1000000
        operation('reserve','request.2',2000000,status=402)
        operation('finalize','request.1',3)
        operation('finalize','request.1',3)
        operation('finalize','request.1',4,status=409)
        assert account()['cash_micros']==2229997 and account()['cash_reserved_micros']==0
        refund={'request_id':'request.1','operation_id':'refund.1','reason':'Test refund'}
        mutate('/admin/billing/refund',refund);mutate('/admin/billing/refund',refund)
        assert account()['cash_micros']==2230000
        grant={'member_id':owner,'amount_micros':1000000,'expires_at':int(time.time())+3600,'operation_id':'grant.1','reason':'Test credit'}
        mutate('/admin/billing/grant',grant);mutate('/admin/billing/grant',grant)
        mutate('/admin/billing/grant',dict(grant,amount_micros=2000000),status=409)
        operation('reserve','request.3',1500000)
        assert account()['credit_reserved_micros']==1000000 and account()['cash_reserved_micros']==500000
        operation('finalize','request.3',1100000)
        assert account()['cash_micros']==2130000 and account()['credit_micros']==0
        sql(db,'UPDATE credit SET expires_at=? WHERE operation_id=?',(int(time.time())-1,'grant.1'))
        mutate('/admin/billing/refund',{'request_id':'request.3','operation_id':'refund.3','reason':'Credit expired'})
        assert account()['cash_micros']==2230000 and account()['credit_micros']==0
        operation('reserve','request.4',500000)
        operation('finalize','request.4',outcome='pending')
        sql(db,'UPDATE reservation SET expires_at=? WHERE request_id=?',(int(time.time())-1,'request.4'))
        assert account()['cash_reserved_micros']==0
        plan={'id':'member','title':'Test membership','duration_days':30,'period_days':30,'credit_micros':500000,'discount_bps':8000,'concurrency_limit':2,'model_ids':'*','enabled':True}
        mutate('/admin/billing/plan',plan)
        sub={'member_id':owner,'plan_id':'member','operation_id':'subscribe.1'}
        mutate('/admin/billing/subscribe',sub);mutate('/admin/billing/subscribe',sub)
        assert account()['credit_micros']==500000 and account()['discount_bps']==8000
        mutate('/admin/billing/subscribe',dict(sub,operation_id='subscribe.2'))
        assert account()['credit_micros']==500000, 'early renewal granted current period twice'
        try:sql(db,'UPDATE ledger SET amount=0')
        except sqlite3.IntegrityError:pass
        else:raise AssertionError('mutable ledger')
        call('POST','/admin/billing/adjust',adjust,admin,status=403)
        call('GET','/api/v1/billing/account',status=401)
        # Restart keeps the authority, balances, original imported cash and period idempotency.
        process.terminate();process.wait(timeout=10);process=launch();ready();admin=login()
        assert account()['cash_micros']==2230000 and account()['credit_micros']==500000
        # Cancellation ends current rights without reclaiming earned credit.
        current=sql(db,"SELECT id FROM subscription WHERE member_id=? AND starts_at<=? ORDER BY id DESC LIMIT 1",(owner,int(time.time())))[0][0]
        state,_,_=call('GET','/admin/billing/state',cookie=admin);admin_headers={'X-CSRF-Token':state['csrf_token'],'Origin':origin}
        mutate('/admin/billing/cancel',{'member_id':owner,'subscription_id':current});mutate('/admin/billing/cancel',{'member_id':owner,'subscription_id':current})
        assert account()['discount_bps']==10000 and account()['credit_micros']==500000
        manage('disable','billing')
        call('GET','/api/v1/balance',headers=bearer,status=503)
        assert sql(target/'db/main.db','SELECT balance FROM member WHERE id=?',(owner,))[0][0]==123
        print('PASS migration, single balance authority, micro-unit cash, atomic holds, insufficient funds, idempotent settle/refund, credit expiry, unknown-cost release, membership renewal, restart and immutable ledger')
    finally:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        print(log.read_text(encoding='utf-8',errors='replace')[-4500:])

if __name__=='__main__':main()
