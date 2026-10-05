"""Native protocol + billing integration against a small deterministic upstream.

No real provider key, payment or stress traffic. Databases are disposable.
"""
import argparse
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import sqlite3
import socket
import subprocess
import threading
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request


class Upstream(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    calls=[]
    def log_message(self,*_):pass
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.calls.append((self.path,body,dict(self.headers)))
        mode=body.get('input','') if self.path=='/responses' else body['messages'][-1]['content']
        if mode=='fail':
            raw=b'{"error":{"message":"sensitive-provider-key"}}'
            self.send_response(401);self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw);return
        usage={'prompt_tokens':100,'completion_tokens':20,'prompt_tokens_details':{'cached_tokens':40},'completion_tokens_details':{'reasoning_tokens':5}}
        if self.path=='/responses':usage={'input_tokens':100,'output_tokens':20,'input_tokens_details':{'cached_tokens':40},'output_tokens_details':{'reasoning_tokens':5}}
        if self.path=='/messages':usage={'input_tokens':60,'cache_read_input_tokens':40,'cache_creation_input_tokens':20,'cache_creation':{'ephemeral_5m_input_tokens':20,'ephemeral_1h_input_tokens':0},'output_tokens':20}
        if body.get('stream'):
            if self.path=='/chat':events=[{'choices':[{'delta':{'content':'你','reasoning_content':'想','tool_calls':[{'index':0,'id':'tool1','type':'function','function':{'name':'read','arguments':'{}'}}]},'finish_reason':None}]},{'choices':[],'usage':usage},'[DONE]']
            elif self.path=='/responses':events=[{'type':'response.output_text.delta','delta':'你'},{'type':'response.completed','response':{'usage':usage}}]
            else:events=[{'type':'message_start','message':{'usage':dict(usage,output_tokens=0)}},{'type':'content_block_delta','delta':{'type':'text_delta','text':'你'}},{'type':'message_delta','delta':{'stop_reason':None},'usage':{'output_tokens':10}},{'type':'message_delta','delta':{'stop_reason':'end_turn'},'usage':{'output_tokens':20}},{'type':'message_stop'}]
            if mode in ('missing','interrupt'):events=events[:1]
            if mode=='badusage':events=[{'choices':[],'usage':dict(usage,prompt_tokens=2)},'[DONE]']
            raw=b''.join(b'data: '+(e.encode() if isinstance(e,str) else json.dumps(e,ensure_ascii=False).encode())+b'\r\n\r\n' for e in events)
            self.send_response(200);self.send_header('Content-Type','text/event-stream');self.send_header('Transfer-Encoding','chunked');self.send_header('x-request-id','provider-fixture');self.end_headers()
            try:
                # Split UTF-8 and SSE line delimiters across actual HTTP chunks.
                points=[1,7,raw.find('你'.encode())+1,len(raw)//2,len(raw)]
                start=0
                for end in sorted(set(p for p in points if p>0)):
                    chunk=raw[start:end];start=end
                    self.wfile.write(f'{len(chunk):x}\r\n'.encode()+chunk+b'\r\n');self.wfile.flush()
                    if end==1:time.sleep(.15)
                if mode=='slow':time.sleep(.5)
                if mode not in ('interrupt','slow'):
                    self.wfile.write(b'0\r\n\r\n');self.wfile.flush()
            except (BrokenPipeError,ConnectionResetError):pass
            self.close_connection=True
        else:
            result={'id':'provider-result','usage':usage}
            if self.path=='/chat':result['choices']=[{'message':{'role':'assistant','content':'answer'},'finish_reason':'stop'}]
            if mode=='missing':result.pop('usage')
            raw=json.dumps(result).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)


def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=ROOT/'xs.exe');p.add_argument('--port',type=int,default=19187);args=p.parse_args()
    target=fixture(args.port);config=json.loads((target/'xs.json').read_text());config['services'][0]['host_default']['devfile']=str(target/'main.c');(target/'xs.json').write_text(json.dumps(config))
    origin=f'http://127.0.0.1:{args.port}'
    (target/'db/identity.json').write_text(json.dumps({'public_origin':'https://production.example'}))
    (target/'preview-identity.json').write_text(json.dumps({'public_origin':origin,'cors_origins':['https://client.example.com']}))
    upstream=ThreadingHTTPServer(('127.0.0.1',0),Upstream);threading.Thread(target=upstream.serve_forever,daemon=True).start();Upstream.calls=[]
    log=target/'gateway.log'
    def launch():
        with log.open('ab') as output:return subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,env=dict(os.environ,XADMIN_IDENTITY_CONFIG=str(target/'preview-identity.json')),stdout=output,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    process=launch()
    def ready():
        for _ in range(100):
            if process.poll() is not None:raise RuntimeError('xs exited')
            try:
                if request(args.port,'GET','/admin/login')[0]==200:return
            except OSError:pass
            time.sleep(.2)
        raise RuntimeError('readiness timeout')
    def call(method,path,body=None,cookie=None,headers=None,status=200):
        actual,h,raw=request(args.port,method,path,body,cookie,headers);assert actual==status,(path,actual,raw)
        return json.loads(raw),h
    def login_admin():
        r,h=call('POST','/admin/login',{'username':USER,'password':client_hash(USER,PASSWORD)});assert r['result'];return h['Set-Cookie'].split(';')[0]
    def manage(name):
        r,_=call('POST','/admin/plugin/enable',{'name':name},admin);assert r['result'],r
    def mutate(path,body,status=200):return call('POST',path,body,admin,admin_headers,status)[0].get('data')
    def sql(db,query,params=()):
        with sqlite3.connect(db) as c:rows=c.execute(query,params).fetchall();c.commit();return rows
    def wallet():return call('GET','/api/v1/billing/account',headers=bearer)[0]['data']
    def receipt(id,headers=None):return call('GET','/api/v1/ai/requests/'+id,headers=headers or bearer)[0]['data']
    def generate(protocol,mode='normal',stream=False,key=None,status=200):
        body={'model':'fixture-model','stream':stream,'xadmin_metadata':{'session_id':'test-session'},**({'input':mode,'max_output_tokens':64} if protocol=='responses' else {'messages':[{'role':'user','content':mode}],'max_tokens':64})}
        headers=dict(bearer,**({'Idempotency-Key':key} if key else {}));path='/api/v1/ai/'+{'chat':'chat/completions','responses':'responses','anthropic':'messages'}[protocol]
        actual,h,raw=request(args.port,'POST',path,body,extra_headers=headers);assert actual==status,(actual,raw)
        return h,raw,body,path
    try:
        ready();admin=login_admin();manage('billing');manage('model-gateway')
        state=call('GET','/admin/model-gateway/state',cookie=admin)[0]['data'];admin_headers={'X-CSRF-Token':state['csrf_token'],'Origin':origin}
        r,_=call('POST','/api/v1/register',{'username':'gateway_member','password':PASSWORD},status=201);owner=r['data']['id']
        r,h=call('POST','/api/v1/login',{'identifier':'gateway_member','password':PASSWORD});bearer={'Authorization':'Bearer '+r['data']['access_token']};member_cookie=h['Set-Cookie'].split(';')[0]
        mutate('/admin/billing/adjust',{'member_id':owner,'amount_micros':1000000,'operation_id':'initial','reason':'Fixture cash'})
        for protocol,path in [('chat','chat'),('responses','responses'),('anthropic','messages')]:
            channel={'id':protocol,'title':protocol,'provider':'fixture','protocol':protocol,'url':f'http://127.0.0.1:{upstream.server_port}/{path}','auth':'x-api-key' if protocol=='anthropic' else 'bearer','anthropic_version':'2023-06-01','timeout_ms':10000,'first_byte_ms':5000,'idle_ms':2000,'max_concurrent':2,'enabled':True,'allow_http':True}
            mutate('/admin/model-gateway/channel',channel);mutate('/admin/model-gateway/credentials',{'channel_id':protocol,'api_key':'fixture-key'})
        rates={'input':2000000,'cache_read':1000000,'cache_write_5m':3000000,'cache_write_1h':6000000,'output':4000000}
        model={'id':'fixture-model','title':'Fixture','description':'Native test','context_window':4096,'max_output':128,'enabled':True,'member_only':False,'tool_calling':True,'vision':False,'reasoning_efforts':'low,medium,high','output_limit_field':'max_tokens','default_protocol':'chat','sale_rates':rates,'cost_rates':{k:v//2 for k,v in rates.items()},'routes':[{'protocol':p,'channel_id':p,'wire_model':'wire-'+p,'priority':0} for p in ('chat','responses','anthropic')]}
        mutate('/admin/model-gateway/model',model)
        catalog=call('GET','/api/v1/ai/catalog',headers=bearer)[0]['data'];assert catalog['models'][0]['available'] and len(catalog['models'][0]['protocols'])==3
        assert 'cost_rates' not in catalog['models'][0]
        # The same-origin cookie path used by native account pages is supported.
        assert request(args.port,'GET','/account/models')[0]==200
        assert call('GET','/api/v1/ai/catalog',cookie=member_cookie)[0]['data']['models'][0]['available']
        assert request(args.port,'OPTIONS','/api/v1/ai/chat/completions',extra_headers={'Origin':'https://client.example.com','Access-Control-Request-Method':'POST','Access-Control-Request-Headers':'Authorization, Content-Type, Idempotency-Key'})[0]==204
        before=wallet()['cash_micros'];charged=0
        for protocol in ('chat','responses','anthropic'):
            for stream in (False,True):
                h,raw,body,path=generate(protocol,stream=stream);id=h.get('X-Request-Id') or h.get('X-Request-ID')
                assert id,(h,raw);r=receipt(id);expected=300 if protocol=='anthropic' else 240;charged+=expected
                assert r['state']=='settled' and r['charged_micros']==expected,r
                assert r['usage']['output_tokens']==20 and r['usage']['cache_read_tokens']==40,r
                assert 'cost_rates' not in r['price_snapshot']
                forwarded=Upstream.calls[-1];assert forwarded[1]['model']=='wire-'+protocol and 'xadmin_metadata' not in forwarded[1]
                if protocol=='chat' and stream:assert forwarded[1]['stream_options']['include_usage'] and 'tool_calls' in raw.decode() and 'reasoning_content' in raw.decode()
                if protocol=='anthropic':assert forwarded[2]['anthropic-version']=='2023-06-01'
        assert wallet()['cash_micros']==before-charged and wallet()['cash_reserved_micros']==0
        # Structural idempotency, no re-execution, and owner isolation.
        h,raw,body,path=generate('chat',key='operation.1');id=h['X-Request-Id'];count=len(Upstream.calls)
        reordered=dict(reversed(list(body.items())));r,_=call('POST',path,reordered,headers=dict(bearer,**{'Idempotency-Key':'operation.1'}),status=409);assert r['error']['request_id']==id
        call('POST',path,dict(body,messages=[{'role':'user','content':'changed'}]),headers=dict(bearer,**{'Idempotency-Key':'operation.1'}),status=422);assert len(Upstream.calls)==count
        # Receipt is private and anonymous catalog/generation is refused.
        call('GET','/api/v1/ai/catalog',status=401);call('POST',path,body,status=401)
        r,_=call('POST','/api/v1/register',{'username':'gateway_other','password':PASSWORD},status=201)
        r,_=call('POST','/api/v1/login',{'identifier':'gateway_other','password':PASSWORD});other={'Authorization':'Bearer '+r['data']['access_token']}
        call('GET','/api/v1/ai/requests/'+id,headers=other,status=404)
        call('POST',path,dict(body,max_tokens=129),headers=bearer,status=400)
        call('POST',path,dict(body,tools=[{'type':'web_search'}]),headers=bearer,status=400)
        call('POST','/api/v1/ai/responses',{'model':'fixture-model','input':'test','previous_response_id':'foreign'},headers=bearer,status=400)
        # HTTP failures are redacted and release holds; absent usage is pending.
        before=wallet()['cash_micros'];_,raw,_,_=generate('chat','fail',status=502);assert b'sensitive-provider-key' not in raw
        assert wallet()['cash_micros']==before and wallet()['cash_reserved_micros']==0
        h,_,_,_=generate('chat','missing');missing=h['X-Request-Id'];assert receipt(missing)['state']=='pending' and wallet()['cash_reserved_micros']>0
        mutate('/admin/billing/resolve',{'request_id':missing,'amount_micros':0,'reason':'No verified usage'})
        assert receipt(missing)['state']=='released' and wallet()['cash_reserved_micros']==0
        # Membership snapshot affects sale only, not provider costs.
        mutate('/admin/billing/plan',{'id':'plus','title':'Plus','duration_days':30,'period_days':30,'credit_micros':0,'discount_bps':8000,'concurrency_limit':2,'model_ids':'*','enabled':True})
        mutate('/admin/billing/subscribe',{'member_id':owner,'plan_id':'plus','operation_id':'plus.1'})
        mutate('/admin/model-gateway/model',dict(model,member_only=True))
        generate('chat',key='member.1');r=call('GET','/api/v1/ai/usage',headers=bearer)[0]['data'];id=r['requests'][0]['id'];assert receipt(id)['charged_micros']==192
        call('POST',path,body,headers=other,status=403)
        # Active price snapshots survive an admin price update during SSE I/O.
        conn=http.client.HTTPConnection('127.0.0.1',args.port,timeout=5);conn.request('POST',path,json.dumps(dict(body,stream=True,messages=[{'role':'user','content':'slow'}])),dict(bearer,**{'Content-Type':'application/json'}));response=conn.getresponse();active=response.headers['X-Request-Id'];response.read(1)
        assert receipt(active)['state']=='running','receipt read erased execution status'
        mutate('/admin/model-gateway/model',dict(model,member_only=True,sale_rates={k:v*2 for k,v in rates.items()}));
        try:response.read()
        except (http.client.IncompleteRead,ConnectionResetError):pass
        conn.close();time.sleep(.3);assert receipt(active)['charged_micros']==192,receipt(active)
        mutate('/admin/billing/refund',{'request_id':active,'operation_id':'refund.active','reason':'Test snapshot refund'});assert receipt(active)['state']=='refunded'
        totals=call('GET','/api/v1/ai/usage',headers=bearer)[0]['data']['totals'][0]
        assert totals['refunded_micros']==192 and totals['gross_charged_micros']-totals['refunded_micros']==totals['charged_micros']
        assert receipt(active)['net_charged_micros']==0
        # Free sale still records known supplier cost without debiting cash.
        mutate('/admin/model-gateway/model',dict(model,sale_rates={k:0 for k in rates}))
        before=wallet()['cash_micros'];h,_,_,_=generate('chat');free=h['X-Request-Id'];assert receipt(free)['charged_micros']==0 and wallet()['cash_micros']==before
        state=call('GET','/admin/model-gateway/state',cookie=admin)[0]['data'];assert next(r for r in state['requests'] if r['id']==free)['cost']==120
        # Billing can stop while a gateway's lock-free network read is active.
        conn=http.client.HTTPConnection('127.0.0.1',args.port,timeout=5);conn.request('POST',path,json.dumps(dict(body,stream=True,messages=[{'role':'user','content':'slow'}])),dict(bearer,**{'Content-Type':'application/json'}));response=conn.getresponse();recover=response.headers['X-Request-Id'];response.read(1)
        r,_=call('POST','/admin/plugin/disable',{'name':'billing'},admin);assert r['result'],r
        try:response.read()
        except (http.client.IncompleteRead,ConnectionResetError):pass
        conn.close();time.sleep(.1);assert request(args.port,'GET','/admin/login')[0]==200
        manage('billing');call('GET','/api/v1/ai/usage',headers=bearer);assert receipt(recover)['state']=='settled'
        # Peer cancellation without final usage remains pending, never zero-priced usage.
        conn=http.client.HTTPConnection('127.0.0.1',args.port,timeout=5);conn.request('POST',path,json.dumps(dict(body,stream=True,messages=[{'role':'user','content':'normal'}])),dict(bearer,**{'Content-Type':'application/json'}));response=conn.getresponse();disconnected=response.headers['X-Request-Id'];response.read(1);response.fp.raw._sock.shutdown(socket.SHUT_RDWR);response.close();conn.close();time.sleep(.4)
        assert receipt(disconnected)['state']=='pending',receipt(disconnected)
        mutate('/admin/billing/resolve',{'request_id':disconnected,'amount_micros':0,'reason':'Client canceled without verified usage'})
        mutate('/admin/model-gateway/model',dict(model,member_only=True,sale_rates={k:v*2 for k,v in rates.items()}))
        # Missing usage is not zero; stale price confirmation and credentials checks.
        call('POST',path,dict(body,xadmin_expected_price_version=catalog['models'][0]['price_version']),headers=bearer,status=409)
        state=call('GET','/admin/model-gateway/state',cookie=admin)[0]['data'];assert all(c['key_configured'] for c in state['channels']) and 'fixture-key' not in json.dumps(state)
        db=target/'db/plugin/model-gateway/plugin.db';billdb=target/'db/plugin/billing/plugin.db'
        assert sql(db,'SELECT COUNT(*) FROM request WHERE usage_json LIKE ? OR price_snapshot LIKE ?',('%Native test%','%fixture-key%'))[0][0]==0
        try:sql(db,'UPDATE price SET sale_json=\'{}\'')
        except sqlite3.IntegrityError:pass
        else:raise AssertionError('mutable price history')
        # Simulate a crash after gateway intent commit but before billing finalize.
        pending=sql(db,"SELECT id FROM request WHERE state='settled' AND id<>? LIMIT 1",(active,))[0][0]
        sql(db,"UPDATE request SET state='settling' WHERE id=?",(pending,))
        # A refund while the consumer is stopped is recovered from durable changes.
        other_settled=sql(db,"SELECT id FROM request WHERE state='settled' AND charged>0 LIMIT 1")[0][0]
        r,_=call('POST','/admin/plugin/disable',{'name':'model-gateway'},admin);assert r['result']
        mutate('/admin/billing/refund',{'request_id':other_settled,'operation_id':'refund.offline','reason':'Consumer stopped'})
        manage('model-gateway');assert receipt(other_settled)['state']=='refunded'
        count=len(Upstream.calls);cash=wallet()['cash_micros'];process.terminate();process.wait(timeout=10);process=launch();ready();admin=login_admin()
        assert receipt(pending)['state']=='settled' and wallet()['cash_micros']==cash and len(Upstream.calls)==count
        assert json.loads((target/'db/identity.json').read_text())['public_origin']=='https://production.example'
        print('PASS native JSON/SSE (3 protocols), UTF-8 fragments, tool/reasoning pass-through, cache billing, cumulative usage, idempotency, owner isolation, validation, error redaction, unknown usage, membership discount, immutable prices, in-flight snapshot/refund and restart without re-generation')
    finally:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        upstream.shutdown();upstream.server_close();print(log.read_text(encoding='utf-8',errors='replace')[-5500:])


if __name__=='__main__':main()
