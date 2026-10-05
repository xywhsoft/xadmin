"""Bounded functional streaming checks through real xs/TCC and local HTTP."""
import argparse
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import subprocess
import threading
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request


class Upstream(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_): pass

    def do_POST(self):
        mode = json.loads(self.rfile.read(int(self.headers['Content-Length'])))['mode']
        if mode == 'before':
            self.send_response(401); self.send_header('Content-Length', '2'); self.end_headers()
            self.wfile.write(b'{}'); self.wfile.flush(); return
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Transfer-Encoding', 'chunked')
        self.end_headers()
        parts = [b'data: {"delta":"', '你'.encode(), b'"}\r\n\r\n', b'data: [DONE]\n\n']
        try:
            for i, part in enumerate(parts):
                self.wfile.write(f'{len(part):x}\r\n'.encode() + part + b'\r\n'); self.wfile.flush()
                if i == 0:
                    time.sleep(2 if mode == 'idle' else .35)
            if mode != 'truncated':
                self.wfile.write(b'0\r\n\r\n'); self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError): pass
        self.close_connection = True


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, default=ROOT/'xs.exe')
    p.add_argument('--port', type=int, default=19184)
    args = p.parse_args()
    target = fixture(args.port)
    shutil.copytree(ROOT/'tests/plugins/stream-sdk', target/'plugin/stream-sdk')
    config = json.loads((target/'xs.json').read_text())
    config['services'][0]['host_default']['devfile'] = str(target/'main.c')
    (target/'xs.json').write_text(json.dumps(config))
    upstream = ThreadingHTTPServer(('127.0.0.1', 0), Upstream)
    threading.Thread(target=upstream.serve_forever, daemon=True).start()
    env = dict(os.environ, STREAM_TEST_URL=f'http://127.0.0.1:{upstream.server_port}/')
    log = target/'stream.log'
    with log.open('wb') as output:
        proc = subprocess.Popen([str(args.exe.resolve()), str(target/'xs.json')], cwd=ROOT, env=env,
            stdout=output, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    try:
        for _ in range(100):
            if proc.poll() is not None: raise RuntimeError('xs exited')
            try:
                if request(args.port,'GET','/admin/login')[0]==200: break
            except OSError: pass
            time.sleep(.2)
        else: raise RuntimeError('readiness timeout')
        status, headers, raw = request(args.port,'POST','/admin/login', {'username':USER,'password':client_hash(USER,PASSWORD)})
        assert status==200 and json.loads(raw)['result'], raw
        cookie=headers['Set-Cookie'].split(';')[0]
        status, _, raw=request(args.port,'POST','/admin/plugin/enable',{'name':'stream-sdk'},cookie)
        assert status==200 and json.loads(raw)['result'], raw
        conn=http.client.HTTPConnection('127.0.0.1',args.port,timeout=4)
        started=time.monotonic(); conn.request('POST','/__test/stream',json.dumps({'mode':'normal'}),{'Content-Type':'application/json'})
        response=conn.getresponse(); first=response.read(16)
        assert time.monotonic()-started < .3, 'first chunk was buffered'
        body=first+response.read(); conn.close()
        assert response.status==200 and body==b'data: {"delta":"'+ '你'.encode()+b'"}\r\n\r\ndata: [DONE]\n\n',body
        assert response.headers['Transfer-Encoding']=='chunked'
        assert request(args.port,'POST','/__test/stream',{'mode':'before'})[0]==401
        for mode in ('truncated','idle'):
            conn=http.client.HTTPConnection('127.0.0.1',args.port,timeout=5)
            conn.request('POST','/__test/stream',json.dumps({'mode':mode}),{'Content-Type':'application/json'})
            response=conn.getresponse()
            try: response.read()
            except (http.client.IncompleteRead, ConnectionResetError): pass
            else: raise AssertionError('incomplete upstream published successful framing')
            finally: conn.close()
        assert request(args.port,'GET','/admin/login')[0]==200
        print('PASS incremental chunked forwarding, UTF-8 split, early first chunk, error status, truncated framing and idle cancellation')
    finally:
        proc.terminate()
        try: proc.wait(timeout=10)
        except subprocess.TimeoutExpired: proc.kill(); proc.wait(timeout=10)
        upstream.shutdown(); upstream.server_close()
        print(log.read_text(encoding='utf-8',errors='replace')[-5000:])

if __name__=='__main__': main()
