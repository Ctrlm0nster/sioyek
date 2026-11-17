#!/usr/bin/env python3
"""Exercise queued rendering/search, latest-query replacement and worker shutdown.

Requires a working Qt display/Vulkan ICD. Uses only temporary profiles and the
repository tutorial. Run against the sanitizer binary for ownership checks.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

spec = importlib.util.spec_from_file_location('render_benchmark',
                                             Path(__file__).with_name('benchmark-rendering.py'))
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--platform', choices=('wayland', 'xcb'), default='wayland')
    parser.add_argument('--validation', action=argparse.BooleanOptionalAction, default=True,
                        help='Enable or disable Vulkan validation layers')
    parser.add_argument('--allow-qt-vuid', action=argparse.BooleanOptionalAction, default=True,
                        help='Allow known upstream Qt 6 bug VUID-12478 when testing with system Qt')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = Path(tempfile.mkdtemp(prefix='sioyek-worker-test-'))
    previous = None
    if shutil.which('hyprctl') and os.environ.get('HYPRLAND_INSTANCE_SIGNATURE'):
        previous = json.loads(bench.hypr('-j', 'activewindow')).get('address')
    try:
        for case in ('idle', 'replace-requests', 'busy-quit', 'busy-close',
                     'cancel-search', 'invalidate-cache', 'zero-cache', 'rename-copy'):
            home = work / case
            config = home / '.config/sioyek'
            config.mkdir(parents=True)
            (config / 'prefs_user.config').write_text(
                'super_fast_search 0\nautomatically_download_matching_paper_name 0\n' +
                ('num_cached_pages 0\n' if case == 'zero-cache' else ''))
            env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
                       XDG_CONFIG_DIRS=str(home / 'empty'), XDG_DATA_HOME=str(home / '.local/share'),
                       XDG_CACHE_HOME=str(home / '.cache'), QT_QPA_PLATFORM=args.platform,
                       QT_FORCE_STDERR_LOGGING='1',
                       SIOYEK_VULKAN_VALIDATION='1' if args.validation else '0')
            if os.environ.get('SIOYEK_TEST_LIBRARY_PATH'):
                env['LD_LIBRARY_PATH'] = os.environ['SIOYEK_TEST_LIBRARY_PATH']
            instance = f'worker-test-{os.getpid()}-{case}'
            pdf = root / 'tutorial.pdf'
            if case == 'rename-copy':
                pdf = home / 'copy.pdf'
                shutil.copy2(root / 'tutorial.pdf', pdf)
            with (work / f'{case}.log').open('w') as log:
                process = subprocess.Popen([str(args.executable.resolve()), '--instance-name', instance,
                                            '--no-auto-config', '--nofocus', str(pdf)],
                                           env=env, stdout=log, stderr=log)
                try:
                    deadline = time.monotonic() + 30
                    while True:
                        if process.poll() is not None:
                            raise RuntimeError(f'{case}: reader exited; inspect {work}')
                        try:
                            state = bench.command(instance, 'get_state_json')[0]
                            if state.get('document_path'):
                                break
                        except (FileNotFoundError, ConnectionRefusedError):
                            pass
                        if time.monotonic() > deadline:
                            raise TimeoutError('Reader startup')
                        time.sleep(.02)
                    bench.command(instance, 'wait_for_indexing_to_finish')
                    bench.command(instance, 'wait_for_renders_to_finish')
                    if case == 'idle':
                        time.sleep(.3)
                    elif case == 'zero-cache':
                        # Let the one-second eviction timer run with N == 0.
                        time.sleep(1.3)
                        bench.command(instance, 'set_view_state', '0 500 1.25 0')
                        bench.command(instance, 'wait_for_renders_to_finish')
                        image = work / 'zero-cache.png'
                        bench.command(instance, 'framebuffer_screenshot', image)
                        assert image.stat().st_size > 1000, 'Zero-cache reader did not render'
                    elif case == 'cancel-search':
                        for request in range(24):
                            bench.command(instance, 'search', 'sioyek' if request % 2 else 'the')
                            bench.command(instance, 'escape')
                            bench.command(instance, 'wait_for_search_to_finish')
                            assert bench.command(instance, 'get_state_json')[0].get('num_search_results', 0) == 0
                        time.sleep(.2)
                        assert bench.command(instance, 'get_state_json')[0].get('num_search_results', 0) == 0
                    elif case == 'invalidate-cache':
                        # This interleaving previously freed the search worker's
                        # document while it was extracting another page.
                        for request in range(60):
                            bench.command(instance, 'search', 'sioyek' if request % 2 else 'the')
                            bench.command(instance, 'setconfig_gamma', '1.05' if request % 2 else '1.0')
                            bench.command(instance, 'set_view_state', f'0 500 {1.1 + request * .004} 0')
                        bench.command(instance, 'wait_for_search_to_finish')
                        bench.command(instance, 'wait_for_renders_to_finish')
                        assert bench.command(instance, 'get_state_json')[0]['num_search_results'] > 0
                    elif case == 'rename-copy':
                        bench.command(instance, 'search', 'the')
                        bench.command(instance, 'rename', 'renamed')
                        bench.command(instance, 'wait_for_renders_to_finish')
                        state = bench.command(instance, 'get_state_json')[0]
                        assert Path(state['document_path']) == home / 'renamed.pdf'
                        assert not pdf.exists() and (home / 'renamed.pdf').is_file()
                        bench.command(instance, 'search', 'sioyek')
                        bench.command(instance, 'wait_for_search_to_finish')
                        assert bench.command(instance, 'get_state_json')[0]['num_search_results'] > 0
                    else:
                        for request in range(48):
                            zoom = 1.0 + (request % 16) * .07
                            bench.command(instance, 'set_view_state', f'0 {400 + request} {zoom} 0')
                            if request % 4 == 0:
                                bench.command(instance, 'search', 'sioyek' if request % 8 == 0 else 'the')
                        if case == 'replace-requests':
                            bench.command(instance, 'set_view_state', '0 500 1.25 0')
                            bench.command(instance, 'wait_for_renders_to_finish')
                            bench.command(instance, 'search', '__sioyek_no_such_worker_test_term__')
                            bench.command(instance, 'wait_for_search_to_finish')
                            assert bench.command(instance, 'get_state_json')[0].get('num_search_results', 0) == 0
                            bench.command(instance, 'search', 'sioyek')
                            bench.command(instance, 'wait_for_search_to_finish')
                            assert bench.command(instance, 'get_state_json')[0].get('num_search_results', 0) > 0
                            image = work / 'completed-render.png'
                            bench.command(instance, 'framebuffer_screenshot', str(image))
                            assert image.stat().st_size > 1000, 'No completed render'
                    bench.command(instance, 'close_window' if case == 'busy-close' else 'quit')
                    assert process.wait(timeout=15) == 0, f'{case}: shutdown/sanitizer failure; inspect {work}'
                finally:
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=15)
            bench.check_reader_log(work / f'{case}.log', validation=args.validation, allow_known_qt_vuid=args.allow_qt_vuid)
            print(f'PASS: {case}', flush=True)
    finally:
        if previous:
            bench.restore_focus(previous)
        print(f'Worker test evidence: {work}', flush=True)


if __name__ == '__main__':
    main()
