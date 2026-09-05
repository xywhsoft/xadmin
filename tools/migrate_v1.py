"""Import v1 assets without overwriting root development or modifying dev/v1.

The C transforms are deliberately mechanical. Business handlers and SQL remain
the baseline; compatibility helpers use the current xrt public API only.
SQLite is copied with backup(), not by copying a live WAL database file.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil
import sqlite3
import tempfile
import os
from contextlib import closing

ROOT = Path(__file__).resolve().parents[1]
OLD = ROOT / 'dev/v1/hosts/xadmin'
MODULES = ('auth', 'member', 'member_auth', 'menu', 'logs', 'option', 'guard')
ROUTES = ('index', 'login', 'auth', 'menu', 'logs', 'brand', 'member', 'api')


def source_text(path):
    return path.read_bytes().decode('utf-8-sig', errors='surrogateescape')


def transform(text):
    text = re.sub(r'\bxvalue\b', 'xvalue*', text)
    text = re.sub(r'(\b\w+(?:->\w+)*)->Type\b', r'xrtValueType(\1)', text)
    text = re.sub(r'(\b\w+(?:->\w+)*)->vArray->Count', r'xrtValueCount(\1)', text)
    text = re.sub(r'xrtPtrArrayGet_Inline\((\w+)->vArray,\s*([^)]*)\)',
                  r'xrtValueArrayGet(\1, (\2) - 1)', text)
    text = re.sub(r'xrtDictWalk\((\w+)->vTable,', r'XA_ValueWalk(\1,', text)
    text = re.sub(r'xrtListWalk\((\w+)->vList,', r'XA_ValueWalk(\1,', text)
    # Shared ownership internals no longer exist. The current request lock
    # protects the inherited mutable SQL statements and application caches.
    text = re.sub(r'^[ \t]*xrtOwnerActivateShared\([^\n]*\);[^\n]*\n', '', text, flags=re.M)
    names = {
        'xrtNow': 'XA_Now', 'xrtTimeToStr': 'XA_TimeToStr',
        'xrtStrToTime': 'XA_StrToTime', 'xrtPathJoin': 'XA_PathJoin',
        'xrtHexEncode': 'XA_HexEncode', 'xrtSHA256': 'xrtSha256',
        'xrtFileReadAll': 'XA_FileReadAll',
        'xrtDictCreate': 'XA_DictCreate', 'xrtDictGet': 'XA_DictGet',
        'xrtDictSet': 'XA_DictSet', 'xrtDictWalk': 'XA_DictWalk',
        'xrtDictDestroy': 'xrtMapDestroy', 'xrtDictCount': 'xrtMapCount',
        'xrtDictClear': 'xrtMapClear', 'xrtDictRemove': 'XA_DictRemove',
        'xrtBufferInit': 'XA_BufferInit',
    }
    for old, new in names.items():
        text = re.sub(r'\b' + old + r'\b', new, text)
    return text


def write_new(path, data):
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def copy_assets(source, target):
    if not source.exists():
        return
    for path in sorted(source.rglob('*')):
        if path.is_file() and not path.name.endswith(('.db-wal', '.db-shm')):
            dest = target / path.relative_to(source)
            if path.suffix == '.db':
                backup_database(path, dest)
            elif not dest.exists():
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, dest)


def backup_database(source, target):
    if target.exists():
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    # Do not leave a half-written main.db that a rerun would mistake for a
    # finished import. Publish the verified snapshot with exclusive creation.
    handle, name = tempfile.mkstemp(prefix='import-', suffix='.db', dir=target.parent)
    os.close(handle)
    temporary = Path(name)
    try:
        with closing(sqlite3.connect(source.as_uri() + '?mode=ro', uri=True)) as src:
            with closing(sqlite3.connect(temporary)) as dst:
                src.backup(dst)
                if dst.execute('PRAGMA quick_check').fetchone()[0] != 'ok':
                    raise RuntimeError('SQLite backup failed validation: ' + str(source))
        try:
            os.link(temporary, target)  # fails rather than overwriting an existing database
        except FileExistsError:
            pass
    finally:
        temporary.unlink(missing_ok=True)


def main():
    if not OLD.is_dir():
        raise SystemExit('dev/v1 baseline is missing')
    for name in ('wwwroot', 'plugin'):
        copy_assets(OLD / name, ROOT / name)
    for name in ('page', 'template', 'options', 'forms', 'uploads'):
        copy_assets(OLD / 'data' / name, ROOT / name)
    for path in (OLD / 'data/install').rglob('*'):
        if path.is_file():
            relative = path.relative_to(OLD / 'data/install')
            if path.suffix == '.db':
                backup_database(path, ROOT / 'db/install' / relative)
            else:
                write_new(ROOT / 'install' / relative, path.read_bytes())
    backup_database(OLD / 'data/db/main.db', ROOT / 'db/main.db')
    write_new(ROOT / 'install.lock', (OLD / 'install.lock').read_bytes())
    for plugin in sorted((OLD / 'data/plugin').iterdir()):
        if not plugin.is_dir():
            continue
        for path in plugin.rglob('*'):
            if not path.is_file() or path.name.endswith(('.db-wal', '.db-shm')):
                continue
            rel = path.relative_to(plugin)
            if path.suffix == '.db':
                backup_database(path, ROOT / 'db/plugin' / plugin.name / rel)
            elif path.name == 'config.json':
                write_new(ROOT / 'options/plugin' / (plugin.name + '.json'), path.read_bytes())
            else:
                write_new(ROOT / 'plugin_data' / plugin.name / rel, path.read_bytes())
    for folder, names in (('module', MODULES), ('route_http', ROUTES)):
        for name in names:
            text = transform(source_text(OLD / 'script' / folder / (name + '.h')))
            target = 'modules' if folder == 'module' else folder
            write_new(ROOT / target / (name + '.h'), text.encode('utf-8', errors='surrogateescape'))
    handlers = set()
    for name in ROUTES:
        handlers.update(re.findall(r'\bvoid\s+(\w+)\s*\(', source_text(OLD / 'script/route_http' / (name + '.h'))))
    registrations = []
    for uri, proc in re.findall(r'AddStaticRouteHTTP\("([^"]+)",\s*(\w+)\)', source_text(OLD / 'script/route.h')):
        if proc in handlers:
            registrations.append(f'\tAddStaticRouteHTTP("{uri}", XHTTP_METHOD_ANY, {proc});')
    route_source = '''/* v1 URI 清单。ANY 保留旧回调内部的方法判断及原有 404 行为。
 * 新接口可以注册单独的方法槽位；迁移不改变既有业务契约。 */
static void RouteHTTP_Init(void)
{
    RouteInfo* brand;
    G_StaticRouteTableHTTP = xrtMapCreate(sizeof(RouteInfo));
''' + '\n'.join(registrations) + '''
    brand = XA_DictGet(G_StaticRouteTableHTTP, "/brand/admin", 12);
    if (brand) { brand->bAuth = false; brand->bAdmin = false; }
}
'''
    write_new(ROOT / 'route.h', route_source.encode())
    # Keep the complete statement declarations so later batches can reuse them.
    define = source_text(OLD / 'script/module/define.h')
    start = define.index('xvalue G_CACHE_RoleAuth')
    end = define.index('void Define_Init')
    write_new(ROOT / 'modules/state.h', transform(define[start:end]).encode('utf-8', errors='surrogateescape'))
    for name in ('includes', 'librarys', 'logs', 'temp'):
        (ROOT / name).mkdir(exist_ok=True)
    runtime = ROOT.parent / 'xserver/release/xs.exe'
    if runtime.exists() and not (ROOT / 'xs.exe').exists():
        shutil.copy2(runtime, ROOT / 'xs.exe')
    manifest = []
    for folder in ('wwwroot', 'page', 'template', 'plugin'):
        src = OLD / folder if folder in ('wwwroot', 'plugin') else OLD / 'data' / folder
        for path in sorted(src.rglob('*')):
            if path.is_file():
                manifest.append({'source': str(path.relative_to(ROOT)).replace('\\', '/'),
                                 'target': folder + '/' + str(path.relative_to(src)).replace('\\', '/'),
                                 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    write_new(ROOT / 'docs/v1-assets.json', (json.dumps(manifest, ensure_ascii=True, indent=2) + '\n').encode())
    print(f'Imported assets: {len(manifest)}; existing root files left untouched.')


if __name__ == '__main__':
    main()
