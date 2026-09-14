# -*- coding: utf-8 -*-
"""sched 家族功能探针：CRUD/启停/复制/示例/预览/批量、shell 与 C 双执行器真跑、
运行日志、导入导出、调度线程自动触发（短间隔任务）。"""
import sys, json, time, sqlite3, subprocess
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import smoke

PORT = 19320

def main():
    target = smoke.fixture(PORT)
    log = open(target / 'sched-probe.log', 'wb')
    proc = subprocess.Popen([str(smoke.ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=smoke.ROOT,
                            stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    r = smoke.request
    try:
        for _ in range(60):
            if proc.poll() is not None:
                raise RuntimeError('server died: ' + open(target / 'sched-probe.log', errors='replace').read()[-2000:])
            try:
                if r(PORT, 'GET', '/admin/login')[0] == 200: break
            except OSError: pass
            time.sleep(0.2)

        s, h, b = r(PORT, 'POST', '/admin/login', {'username': smoke.USER,
            'password': smoke.client_hash(smoke.USER, smoke.PASSWORD)})
        assert json.loads(b)['result'], b
        ck = h['Set-Cookie'].split(';')[0]

        # 视图页
        for p in ('/admin/view/sched', '/admin/view/sched/edit', '/admin/view/sched/dashboard', '/admin/view/sched/log'):
            assert r(PORT, 'GET', p, cookie=ck)[0] == 200, p

        # ---- CRUD：创建 shell 任务 ----
        s, _, b = r(PORT, 'POST', '/admin/sched/task/save', {
            'name': 'probe-shell', 'execType': 'shell', 'shellType': 'cmd',
            'scheduleType': 'once', 'onceAt': int(time.time() * 1000000) + 3600 * 1000000,
            'timeoutSec': 10, 'codeText': '@echo probe-shell-output-%RANDOM%\n'}, cookie=ck)
        ret = json.loads(b)
        assert ret['result'], b
        shell_id = ret['id']

        # 校验失败分支（缺代码）
        s, _, b = r(PORT, 'POST', '/admin/sched/task/save', {
            'name': 'bad', 'execType': 'shell', 'scheduleType': 'once',
            'onceAt': 1, 'codeText': ''}, cookie=ck)
        assert not json.loads(b)['result'], b

        # 详情 / 预览
        s, _, b = r(PORT, 'GET', '/admin/sched/task?id=%d' % shell_id, cookie=ck)
        assert json.loads(b)['result'] and json.loads(b)['data']['name'] == 'probe-shell', b
        s, _, b = r(PORT, 'POST', '/admin/sched/preview', {
            'scheduleType': 'cron', 'cronExpr': '0 */5 * * * *',
            'execType': 'shell', 'codeText': 'x', 'name': 'p'}, cookie=ck)
        preview = json.loads(b)
        assert preview['result'] and len(preview['data']) == 5, b
        # 预览间隔
        s, _, b = r(PORT, 'POST', '/admin/sched/preview', {
            'scheduleType': 'interval', 'intervalValue': 30, 'intervalUnit': 'second',
            'execType': 'shell', 'codeText': 'x', 'name': 'p'}, cookie=ck)
        preview = json.loads(b)
        ats = [x['at'] for x in preview['data']]
        assert all(abs(ats[i+1] - ats[i] - 30000000) <= 2 for i in range(4)), ats

        # 更新
        s, _, b = r(PORT, 'PUT', '/admin/sched/task/save', {
            'id': shell_id, 'name': 'probe-shell-2', 'execType': 'shell', 'shellType': 'cmd',
            'scheduleType': 'once', 'onceAt': int(time.time() * 1000000) + 7200 * 1000000,
            'timeoutSec': 10, 'codeText': '@echo updated\n'}, cookie=ck)
        assert json.loads(b)['result'], b
        s, _, b = r(PORT, 'GET', '/admin/sched/tasks?page=1&limit=10&search=probe-shell-2', cookie=ck)
        assert json.loads(b)['count'] == 1, b

        # ---- 立即运行 shell 任务 → 运行日志出现 success ----
        s, _, b = r(PORT, 'POST', '/admin/sched/task/run?id=%d' % shell_id, cookie=ck)
        assert json.loads(b)['result'], b
        ok = False
        for _ in range(30):
            time.sleep(0.4)
            s, _, b = r(PORT, 'GET', '/admin/sched/logs?page=1&limit=5&taskId=%d' % shell_id, cookie=ck)
            rows = json.loads(b)['data']
            if rows and rows[0]['status'] in ('success', 'failed'):
                assert rows[0]['status'] == 'success', rows[0]
                assert rows[0]['triggerSource'] == 'manual'
                ok = True
                break
        assert ok, 'shell run log missing'

        # ---- C 任务真跑（生成 runner → 独立 xs.exe 子进程） ----
        s, _, b = r(PORT, 'POST', '/admin/sched/task/save', {
            'name': 'probe-c', 'execType': 'c', 'scheduleType': 'once',
            'onceAt': int(time.time() * 1000000) + 3600 * 1000000, 'timeoutSec': 30,
            'codeText': 'int TaskProc(TaskInfo* info) {\n'
                        '    snprintf(info->output, SCHED_OUTPUT_CAP, "c-task-%lld", (long long)info->task_id);\n'
                        '    return 0;\n'
                        '}\n'}, cookie=ck)
        ret = json.loads(b)
        assert ret['result'], b
        c_id = ret['id']
        s, _, b = r(PORT, 'POST', '/admin/sched/task/run?id=%d' % c_id, cookie=ck)
        assert json.loads(b)['result'], b
        ok = False
        for _ in range(60):
            time.sleep(0.5)
            s, _, b = r(PORT, 'GET', '/admin/sched/logs?page=1&limit=5&taskId=%d' % c_id, cookie=ck)
            rows = json.loads(b)['data']
            if rows and rows[0]['status'] in ('success', 'failed'):
                assert rows[0]['status'] == 'success', rows[0]
                ok = True
                break
        assert ok, 'c run log missing'

        # ---- 复制 / 示例 / 批量 ----
        s, _, b = r(PORT, 'POST', '/admin/sched/task/copy?id=%d' % shell_id, cookie=ck)
        assert json.loads(b)['result'], b
        copy_id = json.loads(b)['id']
        s, _, b = r(PORT, 'POST', '/admin/sched/task/example?kind=c', cookie=ck)
        assert json.loads(b)['result'], b
        s, _, b = r(PORT, 'POST', '/admin/sched/task/batch_enable', {'ids': [shell_id, c_id], 'op': 'enable'}, cookie=ck)
        assert json.loads(b)['result'], b
        s, _, b = r(PORT, 'POST', '/admin/sched/task/batch_delete', {'ids': [copy_id], 'op': 'delete'}, cookie=ck)
        assert json.loads(b)['result'], b

        # ---- 调度线程自动触发（3 秒间隔任务，等 8 秒） ----
        s, _, b = r(PORT, 'POST', '/admin/sched/task/save', {
            'name': 'probe-auto', 'execType': 'shell', 'shellType': 'cmd',
            'scheduleType': 'interval', 'intervalValue': 3, 'intervalUnit': 'second',
            'timeoutSec': 10, 'codeText': '@echo auto\n'}, cookie=ck)
        auto_id = json.loads(b)['id']
        s, _, b = r(PORT, 'POST', '/admin/sched/task/enable?id=%d&enabled=1' % auto_id, cookie=ck)
        assert json.loads(b)['result'], b
        time.sleep(8)
        s, _, b = r(PORT, 'GET', '/admin/sched/logs?page=1&limit=10&taskId=%d' % auto_id, cookie=ck)
        runs = json.loads(b)['data']
        assert len(runs) >= 2 and all(x['status'] in ('success', 'running') for x in runs), \
            [x['status'] for x in runs]
        assert all(x['triggerSource'] == 'scheduler' for x in runs), runs[0]

        # ---- 仪表盘 / 导出 / 导入 / 日志清空 ----
        s, _, b = r(PORT, 'GET', '/admin/sched/dashboard', cookie=ck)
        dash = json.loads(b)
        assert dash['result'] and dash['data']['total'] >= 3, b
        s, _, b = r(PORT, 'GET', '/admin/sched/export', cookie=ck)
        exported = json.loads(b)
        assert isinstance(exported, list) and any(t['name'] == 'probe-auto' for t in exported), b[:200]
        # 导入（改名后）
        payload = [dict(t) for t in exported if t['name'] == 'probe-auto']
        payload[0]['name'] = 'probe-imported'
        s, _, b = r(PORT, 'POST', '/admin/sched/import', payload, cookie=ck)
        assert json.loads(b)['result'], b
        # 重复导入同名 → skip
        s, _, b = r(PORT, 'POST', '/admin/sched/import', payload, cookie=ck)
        assert '跳过 1' in json.loads(b)['message'], b
        s, _, b = r(PORT, 'POST', '/admin/sched/logs/clear', {}, cookie=ck)
        assert json.loads(b)['result'], b
        s, _, b = r(PORT, 'GET', '/admin/sched/logs?page=1&limit=10', cookie=ck)
        assert json.loads(b)['count'] == 0, b

        # 停用自动任务，避免 teardown 竞态
        r(PORT, 'POST', '/admin/sched/task/enable?id=%d&enabled=0' % auto_id, cookie=ck)
        print('SCHED PROBE PASS')
    finally:
        proc.terminate()
        try: proc.wait(timeout=15)
        except Exception: proc.kill()
        print('fixture:', target)

if __name__ == '__main__':
    main()
