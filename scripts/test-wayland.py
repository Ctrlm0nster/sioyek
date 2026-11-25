#!/usr/bin/env python3
"""Opt-in native Wayland keyboard smoke test on Hyprland; no config edits.
Requires wtype and hyprctl. Focuses a temporary reader and restores prior focus.
Do not type in other windows while this test runs.
"""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=Path)
parser.add_argument('--onscreen', action='store_true',
                    help='Also check compositor pixels for status/command widget repaints (grim + Pillow)')
parser.add_argument('--allow-qt-vuid', action=argparse.BooleanOptionalAction, default=False,
                    help='Allow known upstream Qt 6.12.0 bug VUID-12478 when testing with unpatched system Qt')
args = parser.parse_args()
if args.onscreen:
    from PIL import Image, ImageChops
for tool in ('wtype', 'hyprctl', *(('grim',) if args.onscreen else ())):
    if not shutil.which(tool):
        sys.exit(f'Missing optional test tool: {tool}')
exe = str(args.executable.resolve())
work = Path(tempfile.mkdtemp(prefix='sioyek-native-wayland-'))
config = work / '.config/sioyek'
config.mkdir(parents=True)
(config / 'keys_user.config').write_text('next_page x\n')
(config / 'prefs_user.config').write_text('zoom_inc_factor 1.5\n' + (
    'show_statusbar_only_when_hovered 0\nstatus_bar_font_size 15\nstatus_bar_format TEST PAGE %{current_page}\n'
    'right_status_bar_format \n' if args.onscreen else ''))
env = dict(os.environ, HOME=str(work), XDG_CONFIG_HOME=str(work / '.config'),
           XDG_CONFIG_DIRS=str(work / 'empty-system-config'),
           XDG_DATA_HOME=str(work / '.local/share'), XDG_CACHE_HOME=str(work / '.cache'),
           QT_QPA_PLATFORM='wayland', QT_FORCE_STDERR_LOGGING='1')
# Instrumented libraries belong to the reader process, not tools such as wtype
# which do not load the sanitizer runtime.
if os.environ.get('SIOYEK_TEST_LIBRARY_PATH'):
    env['LD_LIBRARY_PATH'] = os.environ['SIOYEK_TEST_LIBRARY_PATH']
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
        if args.onscreen:
            command('toggle_fullscreen')
            time.sleep(1)
            client = next(c for c in json.loads(hypr('-j', 'clients')) if c['pid'] == process.pid)
            assert client['fullscreen'], 'Onscreen captures require a fullscreen test reader'
            x, y = client['at']
            width, height = client['size']
            geometry = f'{x},{y} {width}x{height}'
            def onscreen(name):
                # Capture compositor output, without asking Qt to repaint for a grab.
                focus(f'pid:{process.pid}')
                time.sleep(.7)
                current = json.loads(hypr('-j', 'activewindow'))
                assert current['pid'] == process.pid and current['fullscreen'], 'Test lost fullscreen focus'
                assert current['at'] == [x, y] and current['size'] == [width, height], 'Test window moved'
                path = work / f'onscreen-{name}.png'
                subprocess.run(['grim', '-s', '1', '-g', geometry, str(path)], check=True, timeout=10)
                return Image.open(path).convert('RGB')
            command('goto_beginning')
            command('wait_for_renders_to_finish')
            command('wait_for_indexing_to_finish')
            first = onscreen('first')
            first_state = state()
            command('goto_end')
            command('wait_for_renders_to_finish')
            last = onscreen('last')
            assert state()['y_offset'] > first_state['y_offset'], 'Page did not change'
            body = (100, 60, width - 100, height - 60)
            assert ImageChops.difference(first.crop(body), last.crop(body)).getbbox(), 'Canvas did not repaint after navigation'
            bottom = (0, height - 20, min(width, 500), height)
            assert ImageChops.difference(first.crop(bottom), last.crop(bottom)).getbbox(), 'Status label did not repaint'
            # The command editor occupies the top edge, independently of the canvas.
            top = (0, 0, width, 60)
            keys('/')
            keys('repaint-check')
            editor = onscreen('editor')
            assert ImageChops.difference(last.crop(top), editor.crop(top)).getbbox(), 'Command editor did not appear'
            keys('-k', 'Escape')
            dismissed = onscreen('dismissed')
            assert ImageChops.difference(last.crop(top), dismissed.crop(top)).getbbox() is None, 'Dismissed command editor left stale pixels'
            print('PASS: compositor captures show navigation/status changes, command editor appearance and clean dismissal')
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
        assert 'Sioyek renderer: Vulkan' in (work / 'launch.log').read_text(), 'Vulkan backend not confirmed'
        print('PASS: native Wayland window, custom x mapping, j scrolling, / search + Enter')
        def capture(name):
            time.sleep(1)
            command('framebuffer_screenshot', str(work / (name + '.png')))
            assert (work / (name + '.png')).stat().st_size > 1000, 'Empty framebuffer screenshot'
        command('toggle_dark_mode')
        capture('dark')
        command('toggle_custom_color')
        capture('custom')
        command('toggle_custom_color')
        command('rotate_clockwise')
        capture('rotated')
        command('rotate_counterclockwise')
        command('toggle_two_page_mode')
        capture('two-page')
        command('toggle_two_page_mode')
        command('toggle_scratchpad_mode')
        capture('scratchpad')
        command('toggle_scratchpad_mode')
        print('PASS: Vulkan backend confirmed; captured dark/custom/rotation/two-page/scratchpad scenes for inspection')
        command('quit')
        assert process.wait(timeout=10) == 0, 'Reader failed at shutdown; inspect launch.log'
        launch_log = (work / 'launch.log').read_text()
        assert not re.search(r'AddressSanitizer|LeakSanitizer|runtime error:', launch_log), (
            'Sanitizer diagnostics; inspect launch.log')
        if args.allow_qt_vuid:
            launch_log = '\n'.join(line for line in launch_log.splitlines()
                                   if 'VUID-VkImageCreateInfo-initialLayout-12478' not in line)
        assert not re.search(r'Validation Error|VUID-', launch_log), (
            'Vulkan validation diagnostics; inspect launch.log')
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
