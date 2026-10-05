"""Real plugin/member/WSS functional contracts using isolated databases."""
import argparse
import json
import os
from pathlib import Path
import socket
import sqlite3
import ssl
import struct
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request as http_request
from channel_e2e import WebSocket


def run(args):
    target = fixture(args.port, source_db=args.source_db)
    origin = f'{"https" if args.tls else "http"}://127.0.0.1:{args.port}'
    context = None
    if args.tls:
        from sms_unit import native_fixture
        server = native_fixture(target); server.shutdown(); server.server_close()
        config = json.loads((target / 'xs.json').read_text())
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0)); plain_port = listener.getsockname()[1]
        config['services'][0].update(tls=True, port_tls=args.port, port=plain_port)
        config['services'][0]['host_default'].update(tls_cert=str(target/'leaf.pem'), tls_key=str(target/'leaf.key'))
        (target / 'xs.json').write_text(json.dumps(config))
        context = ssl.create_default_context(cafile=target/'ca.pem')
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': origin}))
    log = target / 'server.log'
    clients = []
    with log.open('wb') as output:
        process = subprocess.Popen([str(args.exe.resolve()), str(target/'xs.json')], cwd=ROOT,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    def call(method, path, data=None, bearer=None, cookie=None, expect=200):
        status, headers, raw = http_request(args.port, method, path, data, cookie,
            {'Authorization':'Bearer ' + bearer} if bearer else None, tls_context=context)
        assert status == expect, (path, status, raw)
        value = json.loads(raw)
        assert value.get('code', 0) == (0 if expect < 400 else expect), value
        return value.get('data'), headers

    def manage(action):
        status, _, raw = http_request(args.port, 'POST', '/admin/plugin/' + action,
            {'name':'device-relay'}, admin, tls_context=context)
        assert status == 200 and json.loads(raw)['result'], raw

    def member(name):
        call('POST', '/api/v1/register', {'username':name, 'password':PASSWORD}, expect=201)
        tokens, _ = call('POST', '/api/v1/login', {'identifier':name, 'password':PASSWORD})
        return tokens['access_token']

    def ticket(bearer, id, role='controller', mode='control', secret=None, expect=200):
        data = {'device_id':id, 'role':role}
        if role == 'device': data['device_secret'] = secret
        else: data['mode'] = mode
        return call('POST', '/api/v1/devices/ticket', data, bearer, expect=expect)[0]

    def connect(proof, expect=101):
        ws = WebSocket(args.port, {'Origin':'http://localhost:12345'}, path='/api/v1/devices/connect',
            protocol=proof['protocol'] + ', xadmin.ticket.' + proof['ticket'], expect=expect, tls_context=context)
        if expect == 101: clients.append(ws)
        return ws

    def notice(ws, kind):
        opcode, raw = ws.recv(); assert opcode == 1, (opcode, raw)
        data = json.loads(raw); assert data['type'] == kind and data['version'] == 1, data
        return data

    def closed(ws, code):
        opcode, raw = ws.recv(); assert opcode == 8 and struct.unpack('!H', raw[:2])[0] == code, (opcode, raw)

    try:
        for _ in range(100):
            if process.poll() is not None: raise RuntimeError('xs exited')
            try:
                if http_request(args.port,'GET','/admin/login',tls_context=context)[0] == 200: break
            except OSError: pass
            time.sleep(.2)
        else: raise RuntimeError('readiness timeout')
        status, headers, raw = http_request(args.port,'POST','/admin/login',
            {'username':USER,'password':client_hash(USER,PASSWORD)},tls_context=context)
        assert status == 200 and json.loads(raw)['result'], raw
        admin = headers['Set-Cookie'].split(';')[0]
        manage('enable')
        a, b = member('relay_member_a'), member('relay_member_b')
        call('GET','/api/v1/devices',expect=401)
        id_a, id_b, secret_a, secret_b = (os.urandom(size).hex() for size in (16,16,32,32))
        registration = {'device_id':id_a, 'device_secret':secret_a, 'name':'PC', 'platform':'windows',
                        'app_version':'test-1', 'allow_remote':False}
        call('POST','/api/v1/devices/register',registration,a,expect=201)
        ticket(a,id_a,role='device',secret=secret_a,expect=403)
        call('POST','/api/v1/devices/register',{**registration,'allow_remote':True},a)
        call('POST','/api/v1/devices/register',{**registration,'device_secret':secret_b},a,expect=403)
        call('POST','/api/v1/devices/register',registration,b,expect=409)
        assert call('GET','/api/v1/devices',bearer=b)[0]['devices'] == []
        ticket(b,id_a,expect=404)
        call('POST','/api/v1/devices/revoke',{'device_id':id_a},b,expect=404)
        ticket(a,id_a,role='device',secret=secret_b,expect=403)
        ticket(a,id_a,expect=409)
        proof = ticket(a,id_a,role='device',secret=secret_a)
        native = connect(proof); notice(native,'ready'); connect(proof,expect=401)
        ticket(a,id_a,role='device',secret=secret_a,expect=409)
        listing, _ = call('GET','/api/v1/devices',bearer=a)
        assert listing['devices'][0]['online'] and 'secret_hash' not in listing['devices'][0]
        control = connect(ticket(a,id_a)); ready = notice(control,'ready')
        opened = notice(native,'peer_open'); assert opened['peer_id'] == ready['peer_id'] and opened['mode'] == 'control'
        peer = bytes.fromhex(ready['peer_id'])
        ticket(a,id_a,expect=409)
        viewer = connect(ticket(a,id_a,mode='view')); view_ready = notice(viewer,'ready')
        opened_view = notice(native,'peer_open'); assert opened_view['mode'] == 'view' and opened_view['peer_id'] == view_ready['peer_id']
        marker = b'{"type":"request","id":"opaque-payload-marker","body":"private-test-content"}'
        control.send(1,marker)
        opcode, wire = native.recv(); assert opcode == 2 and wire == b'MDR1'+peer+b'\x01'+marker
        native.send(2,b'MDR1'+peer+b'\x01'+b'{"id":"reply"}')
        assert control.recv() == (1,b'{"id":"reply"}')
        binary = bytes(range(256))*2
        control.send(2,binary); opcode, wire = native.recv()
        assert opcode == 2 and wire == b'MDR1'+peer+b'\x02'+binary
        native.send(2,wire); assert control.recv() == (2,binary)
        # A malicious controller's nested peer header remains opaque payload.
        nested = b'MDR1'+bytes.fromhex(view_ready['peer_id'])+b'\x01payload'
        control.send(2,nested); opcode, wire = native.recv()
        assert wire == b'MDR1'+peer+b'\x02'+nested
        print('PASS all-member registration, proof validation, cross-account isolation, single-use tickets, trusted control/view identity and text/binary forwarding')
        viewer.send(8,struct.pack('!H',1000)); closed(viewer,1000); notice(native,'peer_close')
        call('POST','/api/v1/devices/revoke',{'device_id':id_a},a)
        closed(native,1008); closed(control,1008)
        ticket(a,id_a,role='device',secret=secret_a,expect=403)
        call('POST','/api/v1/devices/register',{**registration,'allow_remote':True},a,expect=403)
        call('POST','/api/v1/devices/register',{**registration,'allow_remote':True,'reactivate':True},a)
        native = connect(ticket(a,id_a,role='device',secret=secret_a)); notice(native,'ready')
        pending = ticket(a,id_a,mode='view')
        control = connect(ticket(a,id_a)); notice(control,'ready'); notice(native,'peer_open')
        manage('reload'); closed(native,1001); closed(control,1001); connect(pending,expect=401)
        listing, _ = call('GET','/api/v1/devices',bearer=a)
        assert listing['devices'][0]['allow_remote'] and not listing['devices'][0]['online']
        native = connect(ticket(a,id_a,role='device',secret=secret_a)); notice(native,'ready')
        print('PASS revoke closes peers, no silent reactivation, plugin reload clears tickets and preserves only device metadata')
        invalid_upgrade = ticket(a,id_a,mode='view')
        connect({**invalid_upgrade,'protocol':'wrong.v1'},expect=403)
        connect(invalid_upgrade,expect=401)
        policy = {'enabled':True,'max_devices_per_member':16,'max_viewers_per_device':3,'ticket_ttl_seconds':5}
        status, _, raw = http_request(args.port,'POST','/admin/plugin/settings',{'name':'device-relay','config':policy},admin,tls_context=context)
        assert status == 200 and json.loads(raw)['result'], raw
        expired = ticket(a,id_a,mode='view')
        time.sleep(5.1);connect(expired,expect=401)
        for _ in range(8): ticket(a,id_a,mode='view')
        ticket(a,id_a,mode='view',expect=429)
        manage('reload');closed(native,1001)
        native = connect(ticket(a,id_a,role='device',secret=secret_a));notice(native,'ready')
        print('PASS invalid upgrade consumes proof; short-lived in-memory tickets expire')
        # Both devices remain isolated when one sends an invalid relay frame.
        call('POST','/api/v1/devices/register',{**registration,'device_id':id_b,'device_secret':secret_b,'allow_remote':True},b,expect=201)
        native_b = connect(ticket(b,id_b,role='device',secret=secret_b)); notice(native_b,'ready')
        native.send(1,b'not-a-relay-frame'); closed(native,1002)
        assert call('GET','/api/v1/devices',bearer=b)[0]['devices'][0]['online']
        call('POST','/api/v1/devices/remove',{'device_id':id_a},a)
        assert call('GET','/api/v1/devices',bearer=a)[0]['devices'] == []
        private_db = target/'db/plugin/device-relay/plugin.db'
        with sqlite3.connect(private_db) as db:
            assert db.execute('PRAGMA user_version').fetchone()[0] == 1
            columns = [row[1] for row in db.execute('PRAGMA table_info(device)')]
            assert 'secret_hash' in columns and not any('payload' in column or 'token' in column for column in columns)
            stored = db.execute('SELECT secret_hash FROM device WHERE id=?',(id_b,)).fetchone()[0]
            import hashlib
            assert stored == hashlib.sha256(secret_b.encode()).hexdigest()
        for path in (private_db,log):
            contents = path.read_bytes()
            for sensitive in (secret_a.encode(),secret_b.encode(),marker,binary,proof['ticket'].encode()):
                assert sensitive not in contents, path
        print('PASS malformed native frame isolates other members; private database contains hashes and metadata only, no payloads/keys/tickets in DB or logs')
    except Exception:
        print(log.read_text(encoding='utf-8',errors='replace')[-7000:])
        raise
    finally:
        for ws in clients: ws.close()
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)
        print('Fixture:',target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,default=ROOT/('xs.exe' if os.name=='nt' else 'xs'))
    parser.add_argument('--port',type=int,default=19291)
    parser.add_argument('--source-db',type=Path)
    parser.add_argument('--tls',action='store_true')
    run(parser.parse_args())
