#!/usr/bin/env python3
"""Exercise fresh defaults and labelled synthetic old schemas in temporary HOMEs.
Never reads/copies personal profiles. Requires an actual Wayland/X11 session.
"""
import argparse
from pathlib import Path
import os
import shutil
import sqlite3
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=Path)
parser.add_argument('--platform', choices=('wayland', 'xcb'), default='wayland')
args = parser.parse_args()
exe = str(args.executable.resolve())
work = Path(tempfile.mkdtemp(prefix='sioyek-default-profile-'))
# A sentinel identity differs from the tutorial opened for the smoke test.
identity = 'a' * 32

def run_reader(home, label):
    config = home/'.config'
    config.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(config),
               XDG_CONFIG_DIRS=str(home/'empty-system-config'),
               XDG_DATA_HOME=str(home/'.local/share'), XDG_CACHE_HOME=str(home/'.cache'),
               QT_QPA_PLATFORM=args.platform)
    instance = f'rc-default-{os.getpid()}-{label}'
    base = [exe, '--instance-name', instance, '--no-auto-config', '--nofocus']
    # Data/config trees are generated here, so no saved tabs or external paths exist.
    with (work/(label+'.log')).open('w') as log:
        proc = subprocess.Popen(base, env=env, stdout=log, stderr=log)
        try:
            time.sleep(3)
            assert proc.poll() is None, f'{label}: reader exited; inspect private log'
            state = subprocess.run(base+['--execute-command', 'get_state_json', '--wait-for-response'],
                                   env=env, capture_output=True, text=True, timeout=15, check=True)
            assert 'document_path' in state.stdout, f'{label}: tutorial not loaded'
            subprocess.run(base+['--execute-command', 'quit', '--wait-for-response'],
                           env=env, capture_output=True, timeout=15, check=True)
            assert proc.wait(timeout=10) == 0, f'{label}: failed shutdown'
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
    return home/'.local/share/sioyek'

def check(data, label):
    with sqlite3.connect(data/'shared.db') as db:
        assert db.execute('pragma integrity_check').fetchone()[0] == 'ok'
        assert db.execute('pragma user_version').fetchone()[0] == 2
        assert db.execute('select desc,offset_y from bookmarks where document_path=?', (identity,)).fetchall() == [('rc-bookmark', 123.5)], label+' bookmark lost'
        assert db.execute('select symbol,offset_y from marks where document_path=?', (identity,)).fetchall() == [('m', 50.0)], label+' mark lost'
        assert db.execute('select desc,type,begin_x,end_y from highlights where document_path=?', (identity,)).fetchall() == [('rc-highlight', 'a', 1.0, 4.0)], label+' highlight lost'
        assert db.execute('select zoom_level,offset_x,offset_y,last_access_time from opened_books where path=?', (identity,)).fetchall() == [(1.5, 5.0, 80.0, '2020-01-01 12:00:00')], label+' reading history lost'
        assert {'offset_x','zoom_level'} <= {r[1] for r in db.execute('pragma table_info(marks)')}
    with sqlite3.connect(data/'local.db') as db:
        assert db.execute('pragma integrity_check').fetchone()[0] == 'ok'
    print('PASS', label, 'integrity, bookmarks, marks, highlights and reading history', flush=True)

fresh = run_reader(work/'fresh', 'fresh-default')
with sqlite3.connect(fresh/'shared.db') as db:
    assert db.execute('pragma integrity_check').fetchone()[0] == 'ok'
    assert db.execute('pragma user_version').fetchone()[0] == 2
print('PASS fresh default profile startup and schema', flush=True)
# Seed a fresh generated profile; there is no claim these are historical user files.
with sqlite3.connect(fresh/'shared.db') as db:
    db.execute('insert into bookmarks(document_path,desc,offset_y) values(?,?,?)', (identity,'rc-bookmark',123.5))
    db.execute('insert into marks(document_path,symbol,offset_y) values(?,?,?)',(identity,'m',50.0))
    db.execute('insert into highlights(document_path,desc,type,begin_x,begin_y,end_x,end_y) values(?,?,?,?,?,?,?)',(identity,'rc-highlight','a',1.,2.,3.,4.))
    db.execute('insert into opened_books(path,zoom_level,offset_x,offset_y,last_access_time) values(?,?,?,?,?)',(identity,1.5,5.,80.,'2020-01-01 12:00:00'))
with sqlite3.connect(fresh/'local.db') as db:
    db.execute('insert into document_hash(path,hash) values(?,?)', ('rc-synthetic-sentinel.pdf',identity))
# Explicit pre-metadata / pre-position schemas, derived from documented migrations.
v1_drop = {'marks':['offset_x','zoom_level'], 'opened_books':['document_name']}
v0_drop = {'bookmarks':['begin_x','begin_y','end_x','end_y','font_size','color_red','color_green','color_blue','font_face','creation_time','modification_time','uuid'],
           'highlights':['text_annot','creation_time','modification_time','uuid'],
           'marks':['creation_time','modification_time','uuid'],
           'links':['src_offset_x','creation_time','modification_time','uuid']}
for version in (2,1,0):
    home = work/f'version-{version}'
    data = home/'.local/share/sioyek'
    shutil.copytree(fresh, data)
    with sqlite3.connect(data/'shared.db') as db:
        if version < 2:
            for table, columns in v1_drop.items():
                for column in columns: db.execute(f'alter table {table} drop column {column}')
        if version == 0:
            for table, columns in v0_drop.items():
                for column in columns: db.execute(f'alter table {table} drop column {column}')
        db.execute(f'pragma user_version={version}')
    run_reader(home, f'synthetic-v{version}')
    check(data, f'synthetic split v{version}')
    # A second startup must not mutate prior bookmarks/history or rerun migration.
    run_reader(home, f'synthetic-v{version}-repeat')
    check(data, f'synthetic split v{version} repeat')
print('Evidence:', work, flush=True)
