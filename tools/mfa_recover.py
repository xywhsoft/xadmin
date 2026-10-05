"""Local MFA recovery. Stop xadmin before applying; never run through HTTP."""
import argparse
import getpass
import json
from pathlib import Path
import sqlite3
import time


def recover(args):
    path = args.db.resolve()
    if not path.is_file():
        raise ValueError('Database does not exist')
    if args.account_id <= 0:
        raise ValueError('Account ID must be positive')
    reason = (args.reason or '').strip()
    if args.apply and (not reason or len(reason) > 200 or '\n' in reason or '\r' in reason):
        raise ValueError('--apply requires a single-line --reason (1-200 characters)')
    mode = 'rw' if args.apply else 'ro'
    with sqlite3.connect(path.as_uri() + '?mode=' + mode, uri=True, isolation_level=None) as db:
        if args.apply:
            db.execute('BEGIN IMMEDIATE')
        try:
            table, name = ('user', 'user') if args.realm == 'admin' else ('member', 'username')
            account = db.execute(f'SELECT id,{name} FROM {table} WHERE id=? AND isDelete=0',
                                 (args.account_id,)).fetchone()
            factor = db.execute('SELECT enabled,version FROM mfa_factor WHERE realm=? AND owner=?',
                                (args.realm, args.account_id)).fetchone()
            if not account or not factor or not factor[0]:
                raise ValueError('Active account with enabled MFA was not found')
            result = {'database': str(path), 'realm': args.realm, 'account_id': account[0],
                      'username': account[1], 'version_before': factor[1],
                      'version_after': factor[1] + 1, 'applied': bool(args.apply)}
            if args.apply:
                now = int(time.time())
                db.execute('UPDATE mfa_factor SET enabled=0,version=version+1,secret=NULL,last_step=-1,'
                           'failures=0,locked_until=0,updated_at=? WHERE realm=? AND owner=? AND version=?',
                           (now, args.realm, args.account_id, factor[1]))
                for table in ('mfa_recovery', 'mfa_setup', 'mfa_challenge'):
                    db.execute(f'DELETE FROM {table} WHERE realm=? AND owner=?',
                               (args.realm, args.account_id))
                if args.realm == 'member':
                    db.execute('UPDATE member_session SET revoked_at=? WHERE member_id=? AND revoked_at=0',
                               (now, args.account_id))
                db.execute('INSERT INTO mfa_audit(realm,owner,event,ip,created_at)VALUES(?,?,?,?,?)',
                           (args.realm, args.account_id, 'offline_disabled: ' + reason,
                            'local:' + getpass.getuser(), now))
                db.execute('COMMIT')
            return result
        except Exception:
            if args.apply and db.in_transaction:
                db.execute('ROLLBACK')
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--db', type=Path, required=True, help='Explicit path to main.db')
    parser.add_argument('--realm', choices=('admin', 'member'), required=True)
    parser.add_argument('--account-id', type=int, required=True)
    parser.add_argument('--reason', help='Recorded audit reason; required with --apply')
    parser.add_argument('--apply', action='store_true', help='Disable MFA; default only previews the target')
    args = parser.parse_args()
    try:
        print(json.dumps(recover(args), ensure_ascii=False, indent=2))
    except (sqlite3.Error, ValueError, OSError) as error:
        parser.exit(1, f'Recovery failed: {error}\n')


if __name__ == '__main__':
    main()
