#!/usr/bin/env python3
"""Measure reader idle scheduling and IPC-observed render/search latency.

Use optimized binaries, temporary profiles and the same validated runtime libraries.
Render-settle waits use Sioyek's existing 100 ms IPC readiness timer; these are
end-to-end measurements, not worker dispatch timestamps, GPU time or FPS.
"""
import argparse
import hashlib
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


def threads(pid):
    result = {}
    for task in Path(f'/proc/{pid}/task').iterdir():
        try:
            status = dict(line.split(':', 1) for line in (task / 'status').read_text().splitlines())
            result[task.name] = {
                'name': status['Name'].strip(),
                'voluntary': int(status['voluntary_ctxt_switches']),
                'involuntary': int(status['nonvoluntary_ctxt_switches']),
                'wait': (task / 'wchan').read_text().strip(),
            }
        except FileNotFoundError:
            pass  # A transient indexing thread may have exited.
    return result


def trial(args, label, binary, index, work):
    home = work / f'{index:02d}-{label}'
    config = home / '.config/sioyek'
    config.mkdir(parents=True)
    (config / 'prefs_user.config').write_text(
        'super_fast_search 0\nlinear_filter 0\nautomatically_download_matching_paper_name 0\n')
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
               XDG_CONFIG_DIRS=str(home / 'empty'), XDG_DATA_HOME=str(home / '.local/share'),
               XDG_CACHE_HOME=str(home / '.cache'), QT_QPA_PLATFORM=args.platform,
               QT_FORCE_STDERR_LOGGING='1', SIOYEK_VULKAN_VALIDATION='1')
    instance = f'sioyek-worker-bench-{os.getpid()}-{index}'
    result = {'label': label, 'render_settle_ms': [], 'first_search_result_ms': [],
              'search_result_counts': [], 'screenshots': []}
    with (home / 'reader.log').open('w') as log:
        process = subprocess.Popen([str(binary), '--instance-name', instance, '--no-auto-config',
                                    '--nofocus', str(args.pdf.resolve())],
                                   env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 30
            while True:
                if process.poll() is not None:
                    raise RuntimeError(f'Reader exited: {home / "reader.log"}')
                try:
                    state = bench.command(instance, 'get_state_json')[0]
                    if state.get('document_path'):
                        break
                except (FileNotFoundError, ConnectionRefusedError):
                    pass
                if time.monotonic() > deadline:
                    raise TimeoutError('Reader startup')
                time.sleep(.02)
            bench.command(instance, 'toggle_fullscreen')
            time.sleep(.25)  # Allow the compositor's asynchronous resize before rendering.
            bench.command(instance, 'set_view_state', '0 500 1.5 0')
            bench.command(instance, 'wait_for_renders_to_finish')
            bench.command(instance, 'wait_for_indexing_to_finish')
            time.sleep(.5)
            state = bench.command(instance, 'get_state_json')[0]
            result['viewport'] = [state['window_width'], state['window_height']]
            warm_frame = home / 'warm-frame.png'
            bench.command(instance, 'framebuffer_screenshot', warm_frame)
            result['framebuffer'] = bench.png_dimensions(warm_frame)
            if args.platform == 'wayland' and shutil.which('hyprctl'):
                client = next(c for c in json.loads(bench.hypr('-j', 'clients'))
                              if c['pid'] == process.pid)
                if client['xwayland']:
                    raise RuntimeError('Expected native Wayland')
            before_cpu, before_tasks = bench.usage(process.pid), threads(process.pid)
            begin = time.perf_counter()
            time.sleep(args.idle_seconds)
            elapsed = time.perf_counter() - begin
            after_cpu, after_tasks = bench.usage(process.pid), threads(process.pid)
            result['idle_seconds'] = elapsed
            result['idle_cpu_seconds'] = after_cpu['cpu_s'] - before_cpu['cpu_s']
            result['idle_threads'] = [
                {'tid': tid, 'name': after_tasks[tid]['name'],
                 'voluntary_switches': after_tasks[tid]['voluntary'] - before_tasks[tid]['voluntary'],
                 'involuntary_switches': after_tasks[tid]['involuntary'] - before_tasks[tid]['involuntary'],
                 'wait_before': before_tasks[tid]['wait'], 'wait_after': after_tasks[tid]['wait']}
                for tid in before_tasks.keys() & after_tasks.keys()]
            for job in range(args.jobs):
                # Identical deterministic offsets from the old 100 ms polling
                # phase prevent accidentally testing only just-awakened workers.
                time.sleep(.035 + (job * 37 % 107) / 1000)
                begin = time.perf_counter()
                bench.command(instance, 'set_view_state', f'0 500 {1.1 + job * .013:.4f} 0')
                bench.command(instance, 'wait_for_renders_to_finish')
                result['render_settle_ms'].append((time.perf_counter() - begin) * 1000)
                path = home / f'render-{job}.png'
                bench.command(instance, 'framebuffer_screenshot', path)
                if bench.png_dimensions(path) != result['framebuffer']:
                    raise RuntimeError('Framebuffer resized during worker benchmark')
                result['screenshots'].append(hashlib.sha256(path.read_bytes()).hexdigest())
            for job in range(0 if args.skip_search else args.jobs):
                time.sleep(.035 + (job * 37 % 107) / 1000)
                begin = time.perf_counter()
                bench.command(instance, 'search', 'sioyek' if job % 2 == 0 else 'the')
                deadline = time.monotonic() + 15
                while bench.command(instance, 'get_state_json')[0].get('num_search_results', 0) == 0:
                    if time.monotonic() > deadline:
                        raise TimeoutError('No search results')
                    time.sleep(.002)
                result['first_search_result_ms'].append((time.perf_counter() - begin) * 1000)
                bench.command(instance, 'wait_for_search_to_finish')
                result['search_result_counts'].append(
                    bench.command(instance, 'get_state_json')[0]['num_search_results'])
            result['memory'] = bench.usage(process.pid)
            bench.command(instance, 'quit')
            if process.wait(timeout=15) != 0:
                raise RuntimeError(f'Reader shutdown failed: {home / "reader.log"}')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=15)
    if 'Sioyek renderer: Vulkan' not in (home / 'reader.log').read_text():
        raise RuntimeError('Vulkan backend not confirmed')
    bench.check_reader_log(home / 'reader.log', allow_known_qt_vuid=args.allow_qt_vuid)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--pdf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--jobs', type=int, default=12)
    parser.add_argument('--idle-seconds', type=float, default=5)
    parser.add_argument('--platform', choices=('wayland', 'xcb'), default='wayland')
    parser.add_argument('--skip-search', action='store_true', help='Image-only PDFs have no searchable text')
    parser.add_argument('--allow-qt-vuid', action=argparse.BooleanOptionalAction, default=False,
                        help='Allow known upstream Qt 6.12.0 bug VUID-12478 when testing with unpatched system Qt')
    args = parser.parse_args()
    if args.rounds < 1 or args.jobs < 1 or args.idle_seconds <= 0:
        parser.error('Counts and idle duration must be positive')
    args.output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='workers-', dir=args.output.resolve()))
    binaries = {'baseline': args.baseline.resolve()}
    if args.candidate:
        binaries['candidate'] = args.candidate.resolve()
    data = {'method': __doc__, 'pdf_sha256': hashlib.sha256(args.pdf.read_bytes()).hexdigest(),
            'validation': True, 'arguments': {k: str(v) if isinstance(v, Path) else v
                                             for k, v in vars(args).items()},
            'binaries': {k: {'path': str(v), 'sha256': hashlib.sha256(v.read_bytes()).hexdigest()}
                         for k, v in binaries.items()}, 'trials': []}
    previous = None
    if shutil.which('hyprctl') and os.environ.get('HYPRLAND_INSTANCE_SIGNATURE'):
        previous = json.loads(bench.hypr('-j', 'activewindow')).get('address')
    try:
        for round_number in range(args.rounds):
            order = tuple(binaries) if round_number % 2 == 0 else tuple(reversed(binaries))
            for label in order:
                result = trial(args, label, binaries[label], len(data['trials']), work)
                data['trials'].append(result)
                if len({tuple(t['viewport']) for t in data['trials']}) != 1:
                    raise RuntimeError('Unequal viewports invalidate comparison')
                if len({tuple(t['framebuffer']) for t in data['trials']}) != 1:
                    raise RuntimeError('Unequal framebuffer sizes invalidate comparison')
                (work / 'results.json').write_text(json.dumps(data, indent=2) + '\n')
                print(f'Completed {len(data["trials"])}: {label}', flush=True)
    finally:
        if previous:
            bench.restore_focus(previous)
        print(f'Worker benchmark evidence: {work}', flush=True)


if __name__ == '__main__':
    main()
