"""Exercise real HTTP writes and inject failures only into the smoke database."""
from contextlib import contextmanager
import json
import sqlite3


def checks(port, target, cookie, login_path, request, client_hash, password):
    database = target / 'db/main.db'

    def query(sql, params=()):
        with sqlite3.connect(database) as db:
            return db.execute(sql, params).fetchall()

    def execute(sql, params=()):
        with sqlite3.connect(database) as db:
            cursor = db.execute(sql, params)
            db.commit()
            return cursor.lastrowid

    def call(method, path, data=None, auth=cookie, success=True):
        status, headers, body = request(port, method, path, data, auth)
        result = json.loads(body)
        assert status == 200 and result.get('result') is success, (method, path, status, result)
        if not success:
            assert 'data' not in result, (path, 'failed write returned an ID', result)
        return headers, result

    def api(method, path, data=None, auth=None, code=0):
        status, _, body = request(port, method, path, data, auth)
        result = json.loads(body)
        assert status == 200 and result.get('code') == code, (path, status, result)
        return result

    @contextmanager
    def fail(table, event='UPDATE', ignore=False):
        # All identifiers are fixed test cases, never values from HTTP input.
        action = 'IGNORE' if ignore else "ABORT, 'injected write failure'"
        execute(f'CREATE TRIGGER smoke_write_failure BEFORE {event} ON "{table}" '
                f'BEGIN SELECT RAISE({action}); END')
        try:
            yield
        finally:
            execute('DROP TRIGGER smoke_write_failure')

    def snapshot(table):
        return query(f'SELECT * FROM "{table}" ORDER BY id')

    def login(username, member=False):
        path = '/api/v1/login' if member else login_path
        status, headers, body = request(port, 'POST', path, {
            'username': username, 'password': client_hash(username, password)})
        result = json.loads(body)
        assert status == 200 and (result.get('code') == 0 if member else result.get('result')), result
        return headers['Set-Cookie'].split(';')[0]

    def access(session, member=False):
        path = '/api/v1/profile' if member else '/admin/auth/user'
        return request(port, 'GET', path, cookie=session)[0]

    def revoked_status(member=False):
        return 401 if member else (302 if login_path == '/admin/login' else 404)

    # Cover every connected management family. Aborted INSERTs must not return
    # stale last_insert_rowid; ignored UPDATEs must not count as successful writes.
    families = (
        ('user', '/admin/auth/user', 'user', {'username': 'write_admin', 'role': 1}),
        ('member', '/admin/member/user', 'username', {'username': 'write_member', 'groupId': 1, 'status': 1}),
        ('role', '/admin/auth/role', 'name', {'name': 'write_role'}),
        ('authGroup', '/admin/auth/group', 'name', {'name': 'write_auth_group'}),
        ('auth', '/admin/auth/auth', 'name', {'name': 'write_auth', 'groupID': 1}),
        ('memberGroup', '/admin/member/group', 'name', {'name': 'write_member_group'}),
        ('memberAuthGroup', '/admin/member/authgroup', 'name', {'name': 'write_member_auth_group'}),
        ('memberAuth', '/admin/member/auth', 'name', {'name': 'write_member_auth', 'groupID': 1}),
        ('menu', '/admin/option/menu', 'title', {'title': 'write_menu', 'parent': 0, 'type': 1}),
    )
    for table, path, key, fields in families:
        payload = {'desc': '', 'authList': '[]', 'authLevel': 0, 'sort': 0, **fields}
        if 'username' in payload:
            payload['password'] = client_hash(payload['username'], password)
        before = snapshot(table)
        for ignore in (False, True):
            with fail(table, 'INSERT', ignore=ignore):
                call('POST', path, payload, success=False)
            assert snapshot(table) == before, table
        call('POST', path, payload)
        value = fields.get(key, fields.get('username'))
        row_id = query(f'SELECT id FROM "{table}" WHERE "{key}"=? AND isDelete=0', (value,))[0][0]
        update = {**payload, 'id': row_id, 'authLevel': 12, 'sort': 12, 'nickname': 'after_write'}
        before = snapshot(table)
        for ignore in (False, True):
            with fail(table, ignore=ignore):
                call('PUT', path, update, success=False)
            assert snapshot(table) == before, table
        call('PUT', path, update)
        call('PUT', path, {**update, 'id': 2147483647}, success=False)
        sessions = [login(fields['username'], table == 'member') for _ in range(2)] if table in ('user', 'member') else []
        for session in sessions:
            assert access(session, table == 'member') == 200
        # Password reset is a separate management write endpoint.
        if sessions:
            reset = {'id': row_id, 'username': fields['username'], 'password': payload['password']}
            before = snapshot(table)
            with fail(table):
                call('POST', path + '/repwd', reset, success=False)
            assert snapshot(table) == before, table
            call('POST', path + '/repwd', reset)
            for session in sessions:
                assert access(session, table == 'member') == revoked_status(table == 'member'), 'repwd kept stale sessions'
            sessions = [login(fields['username'], table == 'member') for _ in range(2)]
            call('POST', path + '/repwd', {**reset, 'id': 2147483647}, success=False)
        before = snapshot(table)
        for ignore in (False, True):
            with fail(table, ignore=ignore):
                call('DELETE', path + '?id=' + str(row_id), success=False)
            assert snapshot(table) == before, table
            for session in sessions:
                assert access(session, table == 'member') == 200, 'failed deletion revoked a session'
        call('DELETE', path + '?id=' + str(row_id))
        assert query(f'SELECT isDelete FROM "{table}" WHERE id=?', (row_id,)) == [(1,)]
        for session in sessions:
            assert access(session, table == 'member') == revoked_status(table == 'member'), 'deleted account session still accepted'
        call('DELETE', path + '?id=2147483647', success=False)
        assert request(port, 'GET', '/admin/auth/user', cookie=cookie)[0] == 200, 'unrelated session revoked'
    print('PASS management writes: SQL failures, ignored/missing rows, retries and account session revocation')

    # Removing the account that owns the current request must not free that
    # request's retained session before the response has finished.
    username = 'write_self_delete'
    _, created = call('POST', '/admin/auth/user', {
        'username': username, 'password': client_hash(username, password), 'role': 1})
    own_cookie = login(username)
    call('DELETE', '/admin/auth/user?id=' + str(created['data']['id']), auth=own_cookie)
    assert access(own_cookie) == revoked_status()
    assert request(port, 'GET', '/admin/auth/user', cookie=cookie)[0] == 200
    print('PASS self-deletion safely completes and revokes the current account session')

    # Moving a category's children and deleting the category is one operation.
    # A failure in either SQL statement must leave both tables unchanged.
    links = (
        ('authGroup', '/admin/auth/group', 'auth', '/admin/auth/auth', 'groupID', {}),
        ('memberAuthGroup', '/admin/member/authgroup', 'memberAuth', '/admin/member/auth', 'groupID', {}),
        ('auth', '/admin/auth/auth', 'uris', None, 'authID', {'groupID': 1}),
        ('memberAuth', '/admin/member/auth', 'uris', None, 'authID', {'groupID': 1}),
    )
    for parent, path, child, child_path, field, extra in links:
        _, created = call('POST', path, {'name': 'write_parent_' + parent, 'desc': '', 'sort': 0, **extra})
        parent_id = created['data']['id']
        if child_path:
            _, created = call('POST', child_path, {'name': 'write_child_' + parent, 'desc': '', 'sort': 0, field: parent_id})
            child_id = created['data']['id']
        else:
            child_id = execute('INSERT INTO uris(authID,uri,desc,isBackend,needAuth,needLog,keepActive,sort,createTime,updateTime) '
                               'VALUES(?,?,?,?,1,0,0,0,0,0)',
                               (parent_id, '/__write/' + parent, '', int(parent == 'auth')))
        for failing_table in (child, parent):
            before_parent, before_child = snapshot(parent), snapshot(child)
            with fail(failing_table):
                call('DELETE', path + '?id=' + str(parent_id), success=False)
            assert snapshot(parent) == before_parent and snapshot(child) == before_child, (parent, 'partial write')
        call('DELETE', path + '?id=' + str(parent_id))
        assert query(f'SELECT "{field}" FROM "{child}" WHERE id=?', (child_id,)) == [(1,)]
    print('PASS permission reassignment/deletion rollback and retry')

    # URI edits must not mutate in-memory authorization on a failed SQL write.
    uri = query('SELECT id,authID,uri,desc,isBackend,needAuth,needLog,keepActive,sort FROM uris WHERE uri=?', ('/admin/auth/user',))[0]
    payload = dict(zip(('id', 'authID', 'uri', 'desc', 'isBackend', 'needAuth', 'needLog', 'keepActive', 'sort'), uri))
    before = snapshot('uris')
    with fail('uris'):
        call('PUT', '/admin/auth/uris', {**payload, 'authID': 2147483647}, success=False)
    assert snapshot('uris') == before
    assert access(cookie) == 200
    call('PUT', '/admin/auth/uris', {**payload, 'authID': 2147483647})
    assert access(cookie) == 403, 'successful URI write did not update authorization'
    call('PUT', '/admin/auth/uris', payload)
    assert access(cookie) == 200
    call('PUT', '/admin/auth/uris', {**payload, 'id': 2147483647}, success=False)

    # Member APIs keep their existing code/msg response contract, including
    # failure cases, and update the session only after a successful SQL write.
    username = 'write_api'
    registration = {'username': username, 'password': client_hash(username, password)}
    before = snapshot('member')
    for ignore in (False, True):
        with fail('member', 'INSERT', ignore=ignore):
            result = api('POST', '/api/v1/register', registration, code=500)
            assert 'data' not in result
        assert snapshot('member') == before
    member_id = api('POST', '/api/v1/register', registration)['data']['id']
    member_cookie = login(username, member=True)
    before = snapshot('member')
    with fail('member'):
        api('PUT', '/api/v1/profile', {'nickname': 'write_api_after'}, member_cookie, code=500)
    assert snapshot('member') == before
    api('PUT', '/api/v1/profile', {'nickname': 'write_api_after'}, member_cookie)
    assert query('SELECT nickname FROM member WHERE id=?', (member_id,)) == [('write_api_after',)]
    new_password = client_hash(username, password + '-changed')
    reset = {'oldPassword': registration['password'], 'newPassword': new_password}
    before = snapshot('member')
    with fail('member'):
        api('POST', '/api/v1/profile/password', reset, member_cookie, code=500)
    assert snapshot('member') == before
    api('POST', '/api/v1/profile/password', reset, member_cookie)
    api('POST', '/api/v1/login', {'username': username, 'password': new_password})
    print('PASS member API registration/profile/password failures and recovery')

    # A log insertion failure must roll back the preceding balance update.
    username = 'write_balance'
    _, created = call('POST', '/admin/member/user', {'username': username, 'password': client_hash(username, password), 'groupId': 1})
    member_id = created['data']['id']
    payload = {'id': member_id, 'type': 0, 'amount': 100, 'remark': 'write regression'}
    before_member, before_logs = snapshot('member'), snapshot('memberBalanceLog')
    with fail('memberBalanceLog', 'INSERT'):
        call('POST', '/admin/member/user/balance', payload, success=False)
    assert snapshot('member') == before_member and snapshot('memberBalanceLog') == before_logs
    call('POST', '/admin/member/user/balance', payload)
    assert query('SELECT balance FROM member WHERE id=?', (member_id,)) == [(100,)]

    # F1: a failed disable must not revoke sessions; a successful one must.
    username = 'write_disable'
    _, created = call('POST', '/admin/member/user', {
        'username': username, 'password': client_hash(username, password),
        'groupId': 1, 'status': 1})
    disable_id = created['data']['id']
    disable = {'id': disable_id, 'groupId': 1, 'authLevel': 0, 'nickname': 'n',
               'email': '', 'phone': '', 'avatar': '', 'status': 0}
    session = login(username, member=True)
    with fail('member'):
        call('PUT', '/admin/member/user', disable, success=False)
    assert access(session, member=True) == 200, 'failed disable revoked a session'
    call('PUT', '/admin/member/user', disable)
    assert access(session, member=True) == 401, 'disable did not revoke the session'
    print('PASS member disable revocation and failed-disable no-op (F1)')

    # Clearing an empty log set is legitimately successful (zero affected rows).
    execute("INSERT INTO logs(user,ip,uri,method,param,body,createTime) VALUES('smoke','','/__write/log','GET','','',1)")
    with fail('logs', 'DELETE'):
        call('POST', '/admin/logs/clear', success=False)
    assert query("SELECT count(*) FROM logs WHERE uri='/__write/log'") == [(1,)]
    call('POST', '/admin/logs/clear')
    assert query("SELECT count(*) FROM logs WHERE uri='/__write/log'") == [(0,)]
    call('POST', '/admin/logs/clear')
    print('PASS URI write isolation, balance/log atomicity and log-clear failures')
