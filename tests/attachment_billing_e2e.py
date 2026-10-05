"""Attachment cash authority, atomic payouts and recovery; disposable fixtures only."""
import argparse
from contextlib import closing, contextmanager
import hashlib
import json
import os
import shutil
import sqlite3
import subprocess
import time
from smoke import ROOT, USER, PASSWORD, client_hash, fixture, request


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=19360)
    args = parser.parse_args()
    original = hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest()
    target = fixture(args.port)
    shutil.copytree(ROOT / 'tests/plugins/billing-sdk', target / 'plugin/billing-sdk')
    main_db = target / 'db/main.db'
    wallet_db = target / 'db/plugin/billing/plugin.db'
    origin = f'http://127.0.0.1:{args.port}'
    (target / 'db/identity.json').write_text(json.dumps({'public_origin': origin}))
    log = target / 'attachment-billing.log'

    def sql(path, statement, params=()):
        with closing(sqlite3.connect(path)) as db:
            rows = db.execute(statement, params).fetchall()
            db.commit()
            return rows

    def launch():
        with log.open('ab') as output:
            return subprocess.Popen([str(ROOT / 'xs.exe'), str(target / 'xs.json')], cwd=ROOT,
                stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    def ready():
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError(log.read_text(errors='replace')[-2000:])
            try:
                if request(args.port, 'GET', '/admin/login')[0] == 200:
                    return
            except OSError:
                pass
            time.sleep(.2)
        raise RuntimeError('server readiness timeout')

    def call(method, path, data=None, cookie=None, headers=None, status=200):
        actual, response_headers, raw = request(args.port, method, path, data, cookie, headers)
        assert actual == status, (path, actual, raw)
        return json.loads(raw), response_headers

    def login():
        result, headers = call('POST', '/admin/login', {'username': USER, 'password': client_hash(USER, PASSWORD)})
        assert result['result']
        return headers['Set-Cookie'].split(';')[0]

    def manage(action, name):
        result, _ = call('POST', '/admin/plugin/' + action, {'name': name}, cookie=admin)
        assert result['result'], result

    def asset(xid, price=100, seller_id=None):
        file = target / 'data/uploads' / (xid + '.txt')
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(b'paid attachment fixture')
        sql(main_db, 'INSERT INTO attachment(xid,filename,ext,mime,path,size,uploaderId,uploaderType,accessType,price,priceType) '
            'VALUES(?,?,?,?,?,?,?,2,2,?,0)', (xid, file.name, 'txt', 'text/plain', file.name, file.stat().st_size,
                                          seller if seller_id is None else seller_id, price))

    def buy(xid, status=200):
        result, _ = call('POST', '/api/v1/attachment/purchase', {'xid': xid}, headers=bearer, status=status)
        assert result['result'] is (status == 200), result
        return result

    def balances():
        return sql(wallet_db, 'SELECT member_id,cash,reserved FROM account ORDER BY member_id')

    def legacy():
        return sql(main_db, 'SELECT id,balance FROM member WHERE id IN (?,?) ORDER BY id', (buyer, seller))

    def account():
        return call('GET', '/api/v1/billing/account', headers=bearer)[0]['data']

    def mutate(path, data):
        result, _ = call('POST', '/admin/billing/' + path, data, cookie=admin, headers=admin_headers)
        assert result['code'] == 0, result

    @contextmanager
    def fail(path, table, event='INSERT', ignore=False):
        action = 'IGNORE' if ignore else "ABORT,'injected failure'"
        sql(path, f'CREATE TRIGGER payment_failure BEFORE {event} ON {table} BEGIN SELECT RAISE({action}); END')
        try:
            yield
        finally:
            sql(path, 'DROP TRIGGER payment_failure')

    process = launch()
    try:
        ready()
        admin = login()
        buyer = call('POST', '/api/v1/register', {'username': 'attachment_buyer', 'password': PASSWORD}, status=201)[0]['data']['id']
        seller = call('POST', '/api/v1/register', {'username': 'attachment_seller', 'password': PASSWORD}, status=201)[0]['data']['id']
        tokens = call('POST', '/api/v1/login', {'identifier': 'attachment_buyer', 'password': PASSWORD})[0]['data']
        bearer = {'Authorization': 'Bearer ' + tokens['access_token']}
        sql(main_db, 'UPDATE member SET balance=1000 WHERE id=?', (buyer,))
        sql(main_db, 'UPDATE member SET balance=200 WHERE id=?', (seller,))
        asset('legacy')
        for table, event in (('attachmentOrder', 'INSERT'), ('attachment', 'UPDATE')):
            for ignore in (False, True):
                before = legacy()
                with fail(main_db, table, event, ignore):
                    buy('legacy', 503)
                assert legacy() == before and sql(main_db, "SELECT count(*) FROM attachmentOrder WHERE attachmentXid='legacy'") == [(0,)]
        buy('legacy')
        buy('legacy')
        income = sql(main_db, "SELECT sellerIncome FROM attachmentOrder WHERE attachmentXid='legacy'")[0][0]
        assert legacy() == [(buyer, 900), (seller, 200 + income)]
        assert sql(main_db, "SELECT salesCount FROM attachment WHERE xid='legacy'") == [(1,)]
        print('PASS legacy debit, seller payout, order and sales roll back together; duplicate purchase')

        manage('enable', 'billing')
        manage('enable', 'billing-sdk')
        state = call('GET', '/admin/billing/state', cookie=admin)[0]['data']
        admin_headers = {'X-CSRF-Token': state['csrf_token'], 'Origin': origin}
        old_legacy = legacy()
        asset('wallet')
        before = balances()
        buy('wallet')
        after = balances()
        assert next(r[1] for r in before if r[0] == buyer) - next(r[1] for r in after if r[0] == buyer) == 1000000
        assert next(r[1] for r in after if r[0] == seller) - next(r[1] for r in before if r[0] == seller) == income * 10000
        buy('wallet')
        assert balances() == after and legacy() == old_legacy
        assert sql(main_db, "SELECT count(*) FROM attachmentOrder WHERE attachmentXid='wallet'") == [(1,)]
        print('PASS canonical wallet debit/payout in micro-units, unchanged legacy balances, one order')

        # A failed payout or ledger/receipt insert must roll back the buyer too.
        for table, event in (('ledger', 'INSERT'), ('cash_payment', 'INSERT'), ('account', 'UPDATE')):
            asset('atomic-' + table)
            for ignore in (False, True):
                before = balances()
                with fail(wallet_db, table, event, ignore):
                    buy('atomic-' + table, 402 if table == 'account' and ignore else 503)
                assert balances() == before
                assert sql(main_db, 'SELECT count(*) FROM attachmentOrder WHERE attachmentXid=?', ('atomic-' + table,)) == [(0,)]
        print('PASS wallet failure injection leaves no partial debit, payout or order')

        # Usage credit and cash reservations cannot be spent on attachments.
        mutate('grant', {'member_id': buyer, 'amount_micros': 50000000, 'expires_at': 0, 'operation_id': 'credit.attachments', 'reason': 'test'})
        cash = account()['cash_micros']
        call('POST', '/__test/billing', {'action': 'reserve', 'request_id': 'hold.cash', 'amount': cash + 50000000}, headers=bearer)
        asset('insufficient')
        buy('insufficient', 402)
        assert account()['cash_micros'] == cash and account()['credit_micros'] == 50000000
        call('POST', '/__test/billing', {'action': 'finalize', 'request_id': 'hold.cash', 'amount': 0, 'outcome': 'released'}, headers=bearer)
        asset('too-expensive', 100000)
        buy('too-expensive', 402)
        assert legacy() == old_legacy
        print('PASS reserved cash and usage credit cannot fund attachment purchases')

        # Persist payment, fail the host order, change the price, then restart.
        asset('recovery')
        before = balances()
        with fail(main_db, 'attachmentOrder'):
            buy('recovery', 503)
            paid = balances()
            buy('recovery', 503)
            assert balances() == paid and paid != before
        assert sql(main_db, "SELECT salesCount FROM attachment WHERE xid='recovery'") == [(0,)]
        sql(main_db, "UPDATE attachment SET price=999,uploaderId=0,uploaderType=1 WHERE xid='recovery'")
        process.terminate()
        process.wait(timeout=10)
        process = launch()
        ready()
        admin = login()
        # Download alone repairs the interrupted order from the durable receipt.
        status, _, body = request(args.port, 'GET', '/attachment?xid=recovery', extra_headers=bearer)
        assert status == 200 and body == b'paid attachment fixture', (status, body)
        buy('recovery')
        assert balances() == paid and legacy() == old_legacy
        assert sql(main_db, "SELECT price,sellerId,sellerIncome FROM attachmentOrder WHERE attachmentXid='recovery'") == [(100, seller, income)]
        assert sql(main_db, "SELECT salesCount FROM attachment WHERE xid='recovery'") == [(1,)]
        print('PASS persisted payment recovers after restart and price/seller changes without a second charge')

        asset('free', 0)
        buy('free')
        status, _, body = request(args.port, 'GET', '/attachment?xid=free', extra_headers=bearer)
        assert status == 200 and body == b'paid attachment fixture'
        assert balances() == paid
        asset('disabled')
        manage('disable', 'billing')
        buy('disabled', 503)
        assert legacy() == old_legacy and balances() == paid
        print('PASS zero-price access and disabled provider refuses stale legacy cash')
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=10)
        assert hashlib.sha256((ROOT / 'db/main.db').read_bytes()).digest() == original
        print('Fixture:', target, '; server stopped; root database unchanged')


if __name__ == '__main__':
    main()
