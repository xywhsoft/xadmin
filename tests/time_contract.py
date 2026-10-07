"""Check historical timestamps and monotonic budgets with the actual xs SDK."""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include <xsbase.h>
#include <stdio.h>
#include <stdlib.h>
#include "xadmin_time.h"
void ServiceInit(XS_HostInfo* host) {
    xtime epoch, earlier, historic;
    xdatetime date;
    if (!XAdmin_TimeFromUnixUs(0, &epoch) || epoch != XRT_TIME_UNIX_EPOCH ||
        !XAdmin_TimeFromUnixUs(-1, &earlier) || earlier != epoch - 1 ||
        !XAdmin_TimeFromUnixUs(1767225600000000LL, &historic) ||
        !xrtTimeSplit(historic, &date) || date.Year != 2026 || date.Month != 1 || date.Day != 1)
        {printf("time conversion failed: %lld %lld %lld %lld/%d/%d\n",(long long)epoch,(long long)earlier,(long long)historic,(long long)date.Year,date.Month,date.Day);exit(2);}
    XAdminDeadline deadline = XAdmin_DeadlineAfterMs(80);
    int64 before = XAdmin_DeadlineRemainingMs(deadline);
    xrtSleep(110);
    if (before <= 0 || before > 81 || !XAdmin_DeadlineExpired(deadline)) {printf("deadline failed: %lld %lld\n",(long long)before,(long long)XAdmin_DeadlineRemainingMs(deadline));exit(3);}
    printf("TIME_CONTRACT {\"now_us\":%lld,\"budget_ms\":%lld}\n",
        (long long)XAdmin_UnixNowUs(), (long long)before);
    exit(0);
}
'''

def run(exe):
    base = ROOT / 'tests/.runtime'; base.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='time-contract-', dir=base) as name:
        site = Path(name)
        shutil.copyfile(ROOT / 'include/xadmin/time.h', site / 'xadmin_time.h')
        (site / 'main.c').write_text(SOURCE, encoding='utf-8')
        (site / 'wwwroot').mkdir()
        with socket.socket() as sock:
            sock.bind(('127.0.0.1',0)); port = sock.getsockname()[1]
        config = {'services':[{'name':'time-contract','enabled':True,'class':'http','ip':'127.0.0.1','port':port,
            'host_default':{'name':'time-contract','enabled':True,'path':str(site/'wwwroot'),
                            'devlang':'c','devfile':str(site/'main.c')}}]}
        (site / 'xs.json').write_text(json.dumps(config))
        begin = time.time()
        proc = subprocess.run([str(exe.resolve()),str(site/'xs.json')],cwd=ROOT,
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20,
            creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        output = proc.stdout.decode(errors='replace')
        assert proc.returncode == 0, (proc.returncode,output)
        row = next(line for line in output.splitlines() if line.startswith('TIME_CONTRACT '))
        result = json.loads(row[len('TIME_CONTRACT '):])
        assert begin - 1 <= result['now_us'] / 1_000_000 <= time.time() + 1, result
        print('PASS Unix epoch, negative floor, historical date, current persisted clock and 80 ms monotonic budget')

if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--exe',type=Path,required=True)
    run(parser.parse_args().exe)
