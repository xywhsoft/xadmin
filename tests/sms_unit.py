"""Exercise real xs/TCC adapters with mocked delivery and independent signatures."""
import argparse
import base64
import hashlib
import hmac
import http.client
import json
import os
import ipaddress
import ssl
import threading
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import tempfile
import time
from urllib.parse import parse_qs, urlsplit, quote

ROOT = Path(__file__).resolve().parents[1]


def native_fixture(target):
    # Test-only dependency, never a runtime dependency of xadmin/xs.
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID
    now = datetime.now(timezone.utc)
    ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'xadmin disposable SMS test CA')])
    ca = (x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name).public_key(ca_key.public_key())
          .serial_number(x509.random_serial_number()).not_valid_before(now-timedelta(minutes=5)).not_valid_after(now+timedelta(days=1))
          .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
          .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
          .sign(ca_key, hashes.SHA256()))
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    cert = (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'localhost')]))
            .issuer_name(ca_name).public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now-timedelta(minutes=5)).not_valid_after(now+timedelta(days=1))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost'), x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]), critical=False)
            .add_extension(x509.ExtendedKeyUsage([x509.oid.ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
            .sign(ca_key, hashes.SHA256()))
    (target/'ca.pem').write_bytes(ca.public_bytes(serialization.Encoding.PEM))
    (target/'leaf.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    (target/'leaf.key').write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *args): pass

        def do_POST(self):
            assert self.rfile.read(int(self.headers['Content-Length'])) == b'{"test":true}'
            if self.path == '/chunked':
                wire = b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n7\r\n{"sent"\r\n6\r\n:true}\r\n0\r\n\r\n'
            elif self.path == '/truncated':
                wire = b'HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\n{"sent":true}'
            elif self.path == '/oversized':
                wire = b'HTTP/1.1 200 OK\r\nContent-Length: 65537\r\nConnection: close\r\n\r\n'
            elif self.path == '/redirect':
                wire = b'HTTP/1.1 302 Found\r\nLocation: https://no-follow.example.test/\r\nContent-Length: 0\r\nConnection: close\r\n\r\n'
            else:
                wire = b'HTTP/1.1 200 OK\r\nContent-Length: 13\r\nConnection: close\r\n\r\n{"sent":true}'
            self.connection.sendall(wire)
            self.close_connection = True

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(target/'leaf.pem', target/'leaf.key')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    (target/'native-url.txt').write_text(f'https://127.0.0.1:{server.server_port}')
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def verify_native(port):
    for case in ('fixed', 'chunked', 'redirect', 'truncated', 'oversized', 'untrusted'):
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=20)
        try:
            conn.request('GET', '/native/' + case)
            response = conn.getresponse()
            value = json.loads(response.read())
        finally:
            conn.close()
        assert value['ok'] == (case in ('fixed', 'chunked', 'redirect')), (case, value)
        if case in ('fixed', 'chunked'): assert value['response'] == '{"sent":true}' and value['status'] == 200, (case, value)
        if case == 'redirect': assert value['status'] == 302
    print('PASS native TLS certificate verification, fixed/chunked responses, truncation/body bounds and no redirect following')


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def mac(key, text):
    return hmac.new(key if isinstance(key, bytes) else key.encode(), text.encode(), hashlib.sha256)


def verify_requests(captures):
    # Validate the first submission for every platform, independently of C code.
    requests = {}
    for item in captures:
        requests.setdefault(urlsplit(item['url']).hostname, item)
    ali = requests['dysmsapi.aliyuncs.com']
    a = ali['headers']
    query = urlsplit(ali['url']).query
    assert query.split('&') == sorted(query.split('&'))
    assert parse_qs(query)['PhoneNumbers'] == ['13800138000']
    assert json.loads(parse_qs(query)['TemplateParam'][0]) == {'code': '123456', 'minutes': '5'}
    assert not ali['body']
    signed = 'host;x-acs-action;x-acs-content-sha256;x-acs-date;x-acs-signature-nonce;x-acs-version'
    canonical = ('POST\n/\n' + query + '\nhost:dysmsapi.aliyuncs.com\nx-acs-action:SendSms\n'
                 'x-acs-content-sha256:' + sha('') + '\nx-acs-date:' + a['x-acs-date'] + '\nx-acs-signature-nonce:' +
                 a['x-acs-signature-nonce'] + '\nx-acs-version:2017-05-25\n\n' + signed + '\n' + sha(''))
    signature = mac('test-secret', 'ACS3-HMAC-SHA256\n' + sha(canonical)).hexdigest()
    assert a['Authorization'].endswith('Signature=' + signature)
    tc = requests['sms.tencentcloudapi.com']
    a = tc['headers']
    date = time.strftime('%Y-%m-%d', time.gmtime(int(a['X-TC-Timestamp'])))
    canonical = ('POST\n/\n\ncontent-type:application/json\nhost:sms.tencentcloudapi.com\n\n'
                 'content-type;host\n' + sha(tc['body']))
    signtext = 'TC3-HMAC-SHA256\n' + a['X-TC-Timestamp'] + '\n' + date + '/sms/tc3_request\n' + sha(canonical)
    key = mac(mac(mac('TC3test-secret', date).digest(), 'sms').digest(), 'tc3_request').digest()
    assert a['Authorization'].endswith('Signature=' + mac(key, signtext).hexdigest())
    data = json.loads(tc['body'])
    assert data['TemplateParamSet'] == ['123456', '5'] and data['SmsSdkAppId'] == '1400000000'
    assert a['X-TC-Version'] == '2021-01-11'
    hw = requests['smsapi.cn-north-4.myhuaweicloud.com']
    wsse = hw['headers']['X-WSSE']
    parts = dict(piece.split('=', 1) for piece in wsse.removeprefix('UsernameToken ').split(','))
    parts = {k: v.strip('"') for k, v in parts.items()}
    digest = hashlib.sha256((parts['Nonce'] + parts['Created'] + 'test-secret').encode()).digest()
    assert parts['PasswordDigest'] == base64.b64encode(digest).decode()
    assert json.loads(parse_qs(hw['body'])['templateParas'][0]) == ['123456', '5']
    bd = requests['smsv3.bj.baidubce.com']
    date = bd['headers']['x-bce-date']
    prefix = 'bce-auth-v1/test-secret/' + date + '/1800'
    canonical = 'POST\n/api/v3/sendSms\n\nhost:smsv3.bj.baidubce.com\nx-bce-date:' + quote(date, safe='~')
    signature = mac(mac('test-secret', prefix).hexdigest(), canonical).hexdigest()
    assert bd['headers']['Authorization'] == prefix + '/host;x-bce-date/' + signature
    yp = requests['sms.yunpian.com']
    form = parse_qs(yp['body'])
    assert form['mobile'] == ['13800138000'] and parse_qs(form['tpl_value'][0]) == {'#code#': ['123456'], '#minutes#': ['5']}
    cl = requests['smssh.253.com']
    data = json.loads(cl['body'])
    md5 = hashlib.md5(b'test-secret').hexdigest()
    signature = mac(md5, ''.join(sorted([md5, data['timestamp'], data['nonce']]))).hexdigest()
    assert cl['headers']['X-QA-Hmac-Signature'] == signature
    assert len(data['nonce']) == 32
    assert json.loads(data['templateParamJson']) == [{'param1': '123456', 'param2': '5'}]
    rl = requests['app.cloopen.com']
    auth = base64.b64decode(rl['headers']['Authorization']).decode()
    account, timestamp = auth.split(':')
    signature = hashlib.md5((account + 'test-secret' + timestamp).encode()).hexdigest().upper()
    assert parse_qs(urlsplit(rl['url']).query)['sig'] == [signature]
    assert json.loads(rl['body'])['datas'] == ['123456', '5']
    sm = requests['api-v4.mysubmail.com']
    assert json.loads(parse_qs(sm['body'])['vars'][0]) == {'code': '123456', 'minutes': '5'}
    # A&B and Unicode notification variables must survive all encodings.
    for item in captures:
        if urlsplit(item['url']).hostname == 'sms.tencentcloudapi.com' and '订单' in item['body']:
            assert json.loads(item['body'])['TemplateParamSet'] == ['订单 A&B+测试']
    print('PASS eight provider request contracts, independent ACS3/TC3/WSSE/BCE/Chuanglan/Ronglian signatures and encoding')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
    parser.add_argument('--port', type=int, default=19293)
    parser.add_argument('--skip-native', action='store_true', help='only provider contracts; native fixture needs cryptography')
    args = parser.parse_args()
    base = ROOT / 'tests/.runtime'
    base.mkdir(exist_ok=True)
    target = Path(tempfile.mkdtemp(prefix='sms-unit-', dir=base))
    (target / 'wwwroot').mkdir()
    config = json.loads((ROOT / 'xs.json').read_text())
    config['services'][0]['port'] = args.port
    config['services'][0]['host_default']['devfile'] = str(ROOT / 'tests/sms_unit.c')
    (target / 'xs.json').write_text(json.dumps(config), encoding='utf-8')
    tls_server = None if args.skip_native else native_fixture(target)
    log = target / 'server.log'
    with log.open('wb') as output:
        process = subprocess.Popen([str(args.exe.resolve()), str(target/'xs.json')], cwd=ROOT,
            stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    try:
        for _ in range(80):
            if process.poll() is not None:
                raise RuntimeError('xs exited')
            conn = http.client.HTTPConnection('127.0.0.1', args.port, timeout=5)
            try:
                conn.request('GET', '/')
                response = conn.getresponse()
                result = json.loads(response.read())
                assert response.status == 200 and result['failures'] == 0, (response.status, result['failures'])
                print('PASS SMS registry/config/result invariants:', result['assertions'], 'assertions')
                verify_requests(result['captured'])
                if tls_server: verify_native(args.port)
                return
            except OSError:
                time.sleep(.2)
            finally:
                conn.close()
        raise RuntimeError('xs readiness timeout')
    finally:
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)
        print(log.read_text(encoding='utf-8', errors='replace')[-5000:])
        if tls_server: tls_server.shutdown(); tls_server.server_close()


if __name__ == '__main__': main()
