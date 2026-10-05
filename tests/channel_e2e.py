"""Bounded functional WebSocket ownership tests, not a load/stress test."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import ssl
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request as http_request


class WebSocket:
    def __init__(self, port, headers, path='/api/v1/channel-test', protocol='xadmin.test.v1', expect=101, tls_context=None):
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=5)
        if tls_context: self.socket = tls_context.wrap_socket(self.socket, server_hostname='127.0.0.1')
        self.socket.settimeout(5)
        self.pending = b''
        key = base64.b64encode(os.urandom(16)).decode()
        lines = [f'GET {path} HTTP/1.1', f'Host: 127.0.0.1:{port}',
                 'Upgrade: websocket', 'Connection: Upgrade', 'Sec-WebSocket-Version: 13',
                 f'Sec-WebSocket-Key: {key}', f'Sec-WebSocket-Protocol: {protocol}']
        lines.extend(f'{k}: {v}' for k, v in headers.items())
        self.socket.sendall(('\r\n'.join(lines) + '\r\n\r\n').encode())
        while b'\r\n\r\n' not in self.pending:
            chunk = self.socket.recv(4096)
            if not chunk: raise AssertionError('connection closed during handshake')
            self.pending += chunk
        head, self.pending = self.pending.split(b'\r\n\r\n', 1)
        assert int(head.split(b' ')[1]) == expect, head
        if expect == 101:
            accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
            assert b'Sec-WebSocket-Accept: ' + accept in head, head
        else: self.close()

    def take(self, size):
        while len(self.pending) < size:
            chunk = self.socket.recv(max(4096, size - len(self.pending)))
            if not chunk: raise EOFError('WebSocket closed')
            self.pending += chunk
        result, self.pending = self.pending[:size], self.pending[size:]
        return result

    def send(self, opcode, data=b'', final=True, masked=True):
        first = opcode | (0x80 if final else 0)
        size = len(data)
        head = bytes([first, (0x80 if masked else 0) | (size if size < 126 else 126 if size < 65536 else 127)])
        if size >= 126: head += struct.pack('!H' if size < 65536 else '!Q', size)
        if masked:
            mask = os.urandom(4)
            head += mask
            data = bytes(value ^ mask[i % 4] for i, value in enumerate(data))
        self.socket.sendall(head + data)

    def recv(self):
        first, size = self.take(2)
        assert not size & 0x80, 'server frame must be unmasked'
        size &= 0x7f
        if size == 126: size = struct.unpack('!H', self.take(2))[0]
        elif size == 127: size = struct.unpack('!Q', self.take(8))[0]
        return first & 0x0f, self.take(size)

    def close(self):
        self.socket.close()


def run(args):
    target = fixture(args.port, source_db=args.source_db)
    if getattr(args,'idle_regression',False):
        config=json.loads((target/'xs.json').read_text())
        config['services'][0]['idle_timeout']=1000
        (target/'xs.json').write_text(json.dumps(config))
    shutil.copytree(ROOT / 'tests/plugins/channel-sdk', target / 'plugin/channel-sdk')
    origin = f'{"https" if args.tls else "http"}://127.0.0.1:{args.port}'
    tls_context = None
    if args.tls:
        from sms_unit import native_fixture
        certificate_server = native_fixture(target)
        certificate_server.shutdown(); certificate_server.server_close()
        config = json.loads((target / 'xs.json').read_text())
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0)); plain_port = listener.getsockname()[1]
        config['services'][0].update(tls=True, port_tls=args.port, port=plain_port)
        config['services'][0]['host_default'].update(tls_cert=str(target/'leaf.pem'), tls_key=str(target/'leaf.key'))
        (target / 'xs.json').write_text(json.dumps(config))
        tls_context = ssl.create_default_context(cafile=target/'ca.pem')
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': origin}))
    log = target / 'server.log'
    clients = []
    with log.open('wb') as output:
        process = subprocess.Popen([str(args.exe.resolve()), str(target / 'xs.json')], cwd=ROOT,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    def call(method, path, data=None, cookie=None, headers=None, expect=200):
        status, response, body = http_request(args.port, method, path, data, cookie, headers, tls_context=tls_context)
        assert status == expect, (status, body)
        return json.loads(body), response

    def connect(headers, **kwargs):
        ws = WebSocket(args.port, headers, tls_context=tls_context, **kwargs)
        if kwargs.get('expect', 101) == 101: clients.append(ws)
        return ws

    def manage(action):
        value, _ = call('POST', '/admin/plugin/' + action, {'name': 'channel-sdk'}, admin)
        assert value['result'], value

    try:
        for _ in range(100):
            if process.poll() is not None: raise RuntimeError('xs exited')
            try:
                if http_request(args.port, 'GET', '/admin/login', tls_context=tls_context)[0] == 200: break
            except OSError: pass
            time.sleep(.2)
        else: raise RuntimeError('readiness timeout')
        value, response = call('POST', '/admin/login', {'username': USER, 'password': client_hash(USER, PASSWORD)})
        assert value['result'], value
        admin = response['Set-Cookie'].split(';')[0]
        manage('enable')
        headers, _ = call('GET','/api/v1/channel-test/headers',headers={'x-test-one':'first','X-Test-Two':'second'})
        assert headers == {'first':'first','second':'second','copied':5,'copy':'first','small':-2}, headers
        import http.client
        conn = (http.client.HTTPSConnection('127.0.0.1',args.port,context=tls_context,timeout=5)
                if tls_context else http.client.HTTPConnection('127.0.0.1',args.port,timeout=5))
        conn.putrequest('GET','/api/v1/channel-test/headers')
        conn.putheader('X-Test-One','first');conn.putheader('x-test-one','duplicate');conn.endheaders()
        headers = json.loads(conn.getresponse().read());conn.close()
        assert headers['first'] == '' and headers['copied'] == -2 and headers['copy'] == '', headers
        print('PASS case-insensitive, terminated request-owned header views, duplicate rejection and bounded copies')
        call('POST', '/api/v1/register', {'username': 'channel_member', 'password': PASSWORD}, expect=201)
        value, response = call('POST', '/api/v1/login', {'identifier': 'channel_member', 'password': PASSWORD})
        bearer = {'Authorization': 'Bearer ' + value['data']['access_token'], 'Origin': origin}
        member = response['Set-Cookie'].split(';')[0]
        connect({}, expect=403)
        connect(bearer, protocol='wrong.v1', expect=403)
        connect(bearer, path='/api/v1/channel-test?bad=1', expect=403)
        connect({'Cookie': member, 'Origin': 'https://evil.example'}, expect=403)
        first = connect(bearer)
        assert first.recv() == (1, b'ready')
        first.send(1, b'hello'); actual = first.recv(); assert actual == (1, b'hello'), actual
        if getattr(args,'idle_regression',False):
            # The plugin owns the accepted protocol's heartbeat/idle policy;
            # the retired HTTP idle timestamp must not close this connection.
            time.sleep(2.5)
            first.send(9,b'after-http-idle')
            assert first.recv()==(10,b'after-http-idle')
            print('PASS taken-over WebSocket survives the HTTP idle deadline')
            return
        first.send(2, bytes(range(256))); assert first.recv() == (2, bytes(range(256)))
        first.send(1, b'frag', final=False)
        first.send(9, b'ping'); assert first.recv() == (10, b'ping')
        first.send(0, b'ment'); assert first.recv() == (1, b'fragment')
        first.send(1, b'reload-self'); assert first.recv() == (1, b'rejected')
        second = connect(bearer); assert second.recv() == (1, b'ready')
        second.send(1, b'parallel'); assert second.recv() == (1, b'parallel')
        first.send(8, struct.pack('!H', 1000)); assert first.recv() == (8, struct.pack('!H', 1000))
        print('PASS handshake rejection, text/binary, fragmentation, ping, two clients, close handshake, reentrant reload rejection')
        malformed = connect(bearer); assert malformed.recv() == (1, b'ready')
        malformed.send(1, b'unmasked', masked=False)
        opcode, body = malformed.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] == 1002
        bounded = connect(bearer); assert bounded.recv() == (1, b'ready')
        bounded.send(1, b'queue-bound')
        opcode, body = bounded.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] == 1013
        stats, _ = call('GET', '/api/v1/channel-test/stats')
        assert stats['backpressure'] == 1, stats
        assert stats['opens'] == 4 and stats['closes'] == 3, stats
        invalid_utf8 = connect(bearer); assert invalid_utf8.recv() == (1, b'ready')
        invalid_utf8.send(1, b'\xff')
        opcode, body = invalid_utf8.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] == 1007
        oversized = connect(bearer); assert oversized.recv() == (1, b'ready')
        oversized.send(2, b'x' * 4097)
        opcode, body = oversized.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] in (1002, 1009)
        call('POST', '/api/v1/logout', {}, headers=bearer)
        opcode, body = second.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] == 1008
        print('PASS protocol error, bounded queue backpressure, exactly-once close callbacks and immediate session revocation')
        value, _ = call('POST', '/api/v1/login', {'identifier': 'channel_member', 'password': PASSWORD})
        bearer['Authorization'] = 'Bearer ' + value['data']['access_token']
        third = connect(bearer); assert third.recv() == (1, b'ready')
        manage('reload')
        opcode, body = third.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] == 1001
        fourth = connect(bearer); assert fourth.recv() == (1, b'ready')
        manage('disable')
        opcode, body = fourth.recv(); assert opcode == 8 and struct.unpack('!H', body[:2])[0] == 1001
        manage('enable')
        fifth = connect(bearer); assert fifth.recv() == (1, b'ready')
        fifth.send(1, b'after-restart'); assert fifth.recv() == (1, b'after-restart')
        print('PASS plugin reload/disable joins channels before code unload; subsequent enable remains usable')
        call('POST', '/__test/reload', {}, expect=202)
        try:
            opcode, body = fifth.recv(); assert opcode == 8
        except EOFError: pass  # xs closes connections belonging to the retired host.
        for _ in range(60):
            try:
                stats, _ = call('GET', '/api/v1/channel-test/stats')
                if stats['opens'] == 0: break
            except OSError: pass
            time.sleep(.1)
        else: raise AssertionError('new host generation did not become ready')
        final = connect(bearer); assert final.recv() == (1, b'ready')
        final.send(1, b'after-host-reload'); assert final.recv() == (1, b'after-host-reload')
        print('PASS host reload retires old channels and new generation remains usable')
    except Exception:
        print(log.read_text(encoding='utf-8', errors='replace')[-7000:])
        raise
    finally:
        for ws in clients: ws.close()
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)
        print('Fixture:', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
    parser.add_argument('--port', type=int, default=19281)
    parser.add_argument('--source-db', type=Path)
    parser.add_argument('--tls', action='store_true')
    parser.add_argument('--idle-regression',action='store_true')
    run(parser.parse_args())
