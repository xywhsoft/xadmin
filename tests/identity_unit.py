"""Run identity invariants through the actual xs/TCC SDK, without user data."""
import argparse
import http.client
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, default=ROOT / ('xs.exe' if os.name == 'nt' else 'xs'))
    p.add_argument('--port', type=int, default=19181)
    args = p.parse_args()
    base = ROOT / 'tests/.runtime'
    base.mkdir(exist_ok=True)
    target = Path(tempfile.mkdtemp(prefix='identity-unit-', dir=base))
    (target / 'wwwroot').mkdir()
    config = json.loads((ROOT / 'xs.json').read_text())
    config['services'][0]['port'] = args.port
    config['services'][0]['host_default']['devfile'] = str(ROOT / 'tests/identity_unit.c')
    (target / 'xs.json').write_text(json.dumps(config), encoding='utf-8')
    log = target / 'server.log'
    with log.open('wb') as output:
        process = subprocess.Popen([str(args.exe.resolve()),str(target/'xs.json')],cwd=ROOT,
                                   stdout=output,stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    try:
        for _ in range(80):
            if process.poll() is not None:
                raise RuntimeError('xs exited')
            connection = http.client.HTTPConnection('127.0.0.1',args.port,timeout=5)
            try:
                connection.request('GET','/')
                response = connection.getresponse()
                body = response.read()
                assert response.status == 200 and body.startswith(b'PASS'), (response.status,body)
                print(body.decode()); return
            except OSError:
                time.sleep(0.2)
            finally:
                connection.close()
        raise RuntimeError('xs readiness timeout')
    finally:
        process.terminate()
        try: process.wait(timeout=10)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=10)
        print(log.read_text(encoding='utf-8',errors='replace')[-7000:])

if __name__ == '__main__': main()
