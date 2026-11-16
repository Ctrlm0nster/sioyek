#!/usr/bin/env python3
"""Opt-in native Wayland keyboard smoke test on Hyprland; no config edits.
Requires wtype and hyprctl. Focuses a temporary reader and restores prior focus.
Do not type in other windows while this test runs.
"""
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

if len(sys.argv) != 2:
    sys.exit('Usage: scripts/test-wayland.py /absolute/path/to/sioyek')
for tool in ('wtype', 'hyprctl'):
    if not shutil.which(tool):
        sys.exit(f'Missing optional test tool: {tool}')
exe = str(Path(sys.argv[1]).resolve())
work = Path(tempfile.mkdtemp(prefix='sioyek-native-wayland-'))
config = work / '.config/sioyek'
config.mkdir(parents=True)
(config / 'keys_user.config').write_text('next_page x\n')
(config / 'prefs_user.config').write_text('zoom_inc_factor 1.5\n')
env = dict(os.environ, HOME=str(work), XDG_CONFIG_HOME=str(work / '.config'),
           XDG_DATA_HOME=str(work / '.local/share'), XDG_CACHE_HOME=str(work / '.cache'),
           QT_QPA_PLATFORM='wayland')
base = [exe, '--instance-name', f'native-wayland-test-{os.getpid()}']
def hypr(*args):
    return subprocess.check_output(['hyprctl', *args], text=True, timeout=10)
def command(name, data=None):
    args = base + ['--nofocus', '--execute-command', name, '--wait-for-response']
    if data is not None:
        args += ['--execute-command-data', data]
    return subprocess.check_output(args, env=env, text=True, timeout=15)
def state():
    return json.loads(next(line for line in command('get_state_json').splitlines()
                           if line.startswith('[{')))[0]
version = re.search(r'Hyprland (\d+)\.(\d+)', hypr('version'))
modern = version is not None and tuple(map(int, version.groups())) >= (0, 55)
def focus(selector):
    if modern:
        hypr('dispatch', 'hl.dsp.focus({window=' + json.dumps(selector) + '})')
    else:
        hypr('dispatch', 'focuswindow', selector)
previous = json.loads(hypr('-j', 'activewindow')).get('address')
with (work / 'launch.log').open('w') as log:
    process = subprocess.Popen(base, env=env, stdout=log, stderr=log)
    try:
        time.sleep(4)
        assert process.poll() is None, 'Reader exited; see launch.log'
        clients = json.loads(hypr('-j', 'clients'))
        client = next(c for c in clients if c['pid'] == process.pid)
        assert not client['xwayland'], 'Test window is not native Wayland'
        focus(f'pid:{process.pid}')
        def keys(*args):
            focus(f'pid:{process.pid}')
            time.sleep(.1)
            assert json.loads(hypr('-j', 'activewindow'))['pid'] == process.pid, 'Test lost focus'
            subprocess.run(['wtype', *args], check=True, timeout=10)
            time.sleep(.5)
        command('goto_beginning')
        time.sleep(1)
        before = state()
        installed_data = Path(exe).parent.parent / 'share/sioyek'
        if installed_data.is_dir():
            assert Path(before['document_path']).resolve() == (installed_data / 'tutorial.pdf').resolve()
            print(f'PASS: tutorial from extracted package: {before["document_path"]}')
        keys('x')
        after = state()
        # At an exact page boundary Sioyek may still report the preceding page.
        assert after['y_offset'] > before['y_offset'], (before, after)
        keys('j')
        assert state()['y_offset'] > after['y_offset'], 'j did not scroll down'
        keys('/')
        keys('sioyek')
        keys('-k', 'Return')
        time.sleep(3)
        assert state().get('num_search_results', 0) > 0, 'No search matches'
        command('framebuffer_screenshot', str(work / 'search.png'))
        print('PASS: native Wayland window, custom x mapping, j scrolling, / search + Enter')
        command('quit')
        assert process.wait(timeout=10) == 0, 'Reader failed at shutdown; inspect launch.log'
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
        if previous:
            try:
                focus(f'address:{previous}')
            except subprocess.CalledProcessError as error:
                print(f'Could not restore prior focus: {error.output}')
        print(f'Test artifacts: {work}')
