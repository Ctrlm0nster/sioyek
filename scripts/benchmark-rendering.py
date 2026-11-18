#!/usr/bin/env python3
"""Compare two local readers on the same PDF, using disposable profiles.

Linux desktop benchmark, not an FPS/GPU timestamp measurement. Uses the existing
RunGuard IPC protocol to avoid launching a new Qt process for every command.
Uses fullscreen temporary readers for equal viewports and restores prior focus.
No host settings, packages, personal profiles, or compositor configuration edits.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import socket
import statistics
import struct
import subprocess
import tempfile
import time


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--opengl', type=Path)
    parser.add_argument('--vulkan', type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--pdf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--frames', type=int, default=90)
    parser.add_argument('--fps', type=int, default=30)
    parser.add_argument('--captures', type=int, default=25)
    parser.add_argument('--platform', choices=('wayland', 'xcb'), default='wayland')
    parser.add_argument('--validation', action=argparse.BooleanOptionalAction, default=True,
                        help='Keep Vulkan validation enabled (default); record the setting in results')
    parser.add_argument('--normal-only', action='store_true',
                        help='Measure normal mode only for profiling/workload sweeps')
    parser.add_argument('--page-turns', type=int, default=0,
                        help='Visit pages 2 through N+1, then revisit up to five early pages')
    parser.add_argument('--rapid-jumps', type=int, default=0,
                        help='Execute rapid jumps forward and back without waiting between moves, then measure final render wait')
    parser.add_argument('--rapid-destinations', type=int, default=4,
                        help='Number of distinct destination pages to test in rapid jump workload')
    args = parser.parse_args()
    if not ((args.opengl and args.vulkan and not args.baseline and not args.candidate) or
            (args.baseline and not args.opengl and not args.vulkan)):
        parser.error('Supply --baseline [--candidate] or --opengl/--vulkan')
    if min(args.rounds, args.frames, args.fps, args.captures) < 1:
        parser.error('Trial counts and cadence must be positive')
    if args.page_turns < 0:
        parser.error('Page turn count must be nonnegative')
    if args.rapid_jumps < 0:
        parser.error('Rapid jumps count must be nonnegative')
    if args.rapid_destinations < 1:
        parser.error('Rapid destinations count must be positive')
    return args


def wire_arguments(values):
    # utils.cpp serialize_string_array: big-endian int32 + length-prefixed
    # UTF-16 QStrings. Requests stay below 4 KiB to match RunGuard::readAll().
    encoded = [value.encode('utf-16-be') for value in values]
    result = struct.pack('>I', len(encoded))
    result += b''.join(struct.pack('>I', len(value)) + value for value in encoded)
    if len(result) >= 4096:
        raise ValueError('IPC request exceeds benchmark packet limit')
    return result


def command(instance, name, data=None):
    values = ['sioyek', '--instance-name', instance, '--nofocus',
              '--execute-command', name, '--wait-for-response']
    if data is not None:
        values += ['--execute-command-data', str(data)]
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as channel:
        channel.settimeout(20)
        channel.connect(str(Path(tempfile.gettempdir()) / instance))
        channel.sendall(wire_arguments(values))
        response = b''
        while len(response) < 1024 * 1024:
            part = channel.recv(65536)
            if not part:
                if name in ('quit', 'close_window'):
                    return None  # process exit can close IPC before a reply
                raise RuntimeError(f'{name}: reader disconnected without a response')
            response += part
            if response == b'<NULL>':
                return None
            try:
                return json.loads(response)
            except (ValueError, UnicodeError):
                pass
        raise RuntimeError(f'{name}: response exceeds benchmark limit')


def usage(pid):
    # /proc/pid/stat fields 14/15 are process user/system CPU ticks.
    fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    cpu = (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
    status = dict(line.split(':', 1) for line in Path(f'/proc/{pid}/status').read_text().splitlines())
    return {'cpu_s': cpu, 'rss_mib': int(status['VmRSS'].split()[0]) / 1024,
            'peak_rss_mib': int(status['VmHWM'].split()[0]) / 1024}


def png_dimensions(path):
    with path.open('rb') as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b'\x89PNG\r\n\x1a\n' or header[12:16] != b'IHDR':
        raise RuntimeError(f'Invalid framebuffer PNG: {path}')
    return list(struct.unpack('>II', header[16:24]))


def check_reader_log(path, validation=True, allow_known_qt_vuid=True):
    text = path.read_text()
    if allow_known_qt_vuid:
        # Ignore known upstream Qt 6 bug VUID-VkImageCreateInfo-initialLayout-12478
        # (Qt's QVkTexture::create() creates optimal-tiled images with VK_IMAGE_LAYOUT_PREINITIALIZED)
        text = re.sub(r'VUID-VkImageCreateInfo-initialLayout-12478', '', text)
    patterns = r'AddressSanitizer|LeakSanitizer|runtime error:'
    if validation:
        patterns = r'Validation Error|VUID-|' + patterns
    if re.search(patterns, text):
        raise RuntimeError(f'Validation/sanitizer diagnostics: {path}')


def checked_view(instance, offset):
    state = command(instance, 'get_state_json')[0]
    expected = {'x_offset': 0, 'y_offset': offset, 'zoom_level': 1.5}
    if (any(abs(state[key] - value) > .001 for key, value in expected.items()) or
            state.get('selected_text') or state.get('selected_rect_absolute') or
            state.get('searching') or state.get('ruler_active')):
        raise RuntimeError(f'Unexpected view/input state invalidates trial: {state}')
    return state


def hypr(*args):
    return subprocess.check_output(['hyprctl', *args], text=True, timeout=10)


def restore_focus(address):
    if not address:
        return
    version = hypr('version')
    # Hyprland 0.55+ uses Lua dispatchers; keep the older command as fallback.
    match = re.search(r'Hyprland (\d+)\.(\d+)', version)
    modern = match and tuple(map(int, match.groups())) >= (0, 55)
    if modern:
        hypr('dispatch', 'hl.dsp.focus({window=' + json.dumps('address:' + address) + '})')
    else:
        hypr('dispatch', 'focuswindow', 'address:' + address)


def percentile_95(values):
    if not values:
        return 0.0
    if len(values) == 1:
        return values[0]
    s = sorted(values)
    k = (len(s) - 1) * 0.95
    f = int(k)
    c = min(f + 1, len(s) - 1)
    weight = k - f
    return s[f] * (1.0 - weight) + s[c] * weight


def measure_page_turns(instance, pid, pages):
    samples = []
    for page in pages:
        start = time.perf_counter()
        command(instance, 'set_state_json', json.dumps({'page_number': page}))
        command(instance, 'wait_for_renders_to_finish')
        wait_ms = (time.perf_counter() - start) * 1000
        state = command(instance, 'get_state_json')[0]
        if state['page_number'] != page:
            raise RuntimeError(f'Expected page {page + 1}, got {state["page_number"] + 1}')
        samples.append({'page': page + 1, 'render_wait_ms': wait_ms, **usage(pid)})
    return samples


def measure_rapid_jumps(instance, pid, jump_targets):
    """Execute rapid jumps forward and back without waiting between moves,
    then measure how long the final visible page takes to render."""
    if not jump_targets:
        return None
    for page in jump_targets[:-1]:
        command(instance, 'set_state_json', json.dumps({'page_number': page}))
        time.sleep(0.01)

    final_page = jump_targets[-1]
    start = time.perf_counter()
    command(instance, 'set_state_json', json.dumps({'page_number': final_page}))
    command(instance, 'wait_for_renders_to_finish')
    wait_ms = (time.perf_counter() - start) * 1000

    state = command(instance, 'get_state_json')[0]
    if state['page_number'] != final_page:
        raise RuntimeError(f'Expected final page {final_page + 1}, got {state["page_number"] + 1}')

    return {
        'jumps': [p + 1 for p in jump_targets],
        'final_page': final_page + 1,
        'final_render_wait_ms': wait_ms,
        **usage(pid)
    }


def measure_isolated_jump(instance, pid, reset_page, target_page):
    """Measure how long a direct jump to target_page takes from an idle/settled state."""
    command(instance, 'set_state_json', json.dumps({'page_number': reset_page}))
    command(instance, 'wait_for_renders_to_finish')
    time.sleep(1.0)
    start = time.perf_counter()
    command(instance, 'set_state_json', json.dumps({'page_number': target_page}))
    command(instance, 'wait_for_renders_to_finish')
    wait_ms = (time.perf_counter() - start) * 1000
    state = command(instance, 'get_state_json')[0]
    if state['page_number'] != target_page:
        raise RuntimeError(f'Expected page {target_page + 1}, got {state["page_number"] + 1}')
    return wait_ms


def trial(args, backend, binary, trial_index, work):
    home = work / f'{trial_index:02d}-{backend}'
    config = home / '.config/sioyek'
    config.mkdir(parents=True)
    (config / 'prefs_user.config').write_text('linear_filter 0\n')
    environment = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
                       XDG_CONFIG_DIRS=str(home / 'empty-system-config'),
                       XDG_DATA_HOME=str(home / '.local/share'), XDG_CACHE_HOME=str(home / '.cache'),
                       QT_QPA_PLATFORM=args.platform, QT_FORCE_STDERR_LOGGING='1',
                       SIOYEK_VULKAN_VALIDATION='1' if args.validation else '0')
    instance = f'sioyek-perf-{os.getpid()}-{trial_index}'
    result = {'backend': backend, 'round': trial_index // 2 + 1, 'samples': {}}
    with (home / 'reader.log').open('w') as log:
        start = time.perf_counter()
        process = subprocess.Popen([str(binary), '--instance-name', instance,
                                    '--no-auto-config', '--nofocus', str(args.pdf.resolve())],
                                   env=environment, stdout=log, stderr=log)
        try:
            deadline = start + 30
            state = None
            while time.perf_counter() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'{backend} exited early: {home / "reader.log"}')
                try:
                    state = command(instance, 'get_state_json')[0]
                    if state.get('document_path'):
                        break
                except (FileNotFoundError, ConnectionRefusedError):
                    pass
                time.sleep(.02)
            if not state or Path(state.get('document_path', '')).resolve() != args.pdf.resolve():
                raise RuntimeError('Benchmark PDF did not open')
            result['startup_to_document_ms'] = (time.perf_counter() - start) * 1000
            if shutil.which('hyprctl') and os.environ.get('HYPRLAND_INSTANCE_SIGNATURE'):
                client = next(c for c in json.loads(hypr('-j', 'clients')) if c['pid'] == process.pid)
                if args.platform == 'wayland' and client['xwayland']:
                    raise RuntimeError('Benchmark reader is not native Wayland')
            command(instance, 'toggle_fullscreen')
            time.sleep(.6)
            command(instance, 'set_view_state', '0 500 1.5 0')
            command(instance, 'wait_for_renders_to_finish')
            screenshot = home / 'first-frame.png'
            command(instance, 'framebuffer_screenshot', screenshot)
            if not screenshot.is_file() or screenshot.stat().st_size < 1000:
                raise RuntimeError('Reader did not produce a usable framebuffer')
            result['startup_to_sized_capture_ms'] = (time.perf_counter() - start) * 1000
            state = command(instance, 'get_state_json')[0]
            result['viewport'] = [state['window_width'], state['window_height']]
            result['framebuffer'] = png_dimensions(screenshot)
            result['document_checksum'] = state['document_checksum']
            if backend != 'opengl' and 'Sioyek renderer: Vulkan' not in (home / 'reader.log').read_text():
                raise RuntimeError('Vulkan backend was not confirmed')
            # Populate page/texture caches before the measured paced scroll.
            for offset in (340, 660, 500):
                command(instance, 'set_view_state', f'0 {offset} 1.5 0')
                command(instance, 'wait_for_renders_to_finish')
            command(instance, 'wait_for_indexing_to_finish')
            result['startup_to_index_ready_ms'] = (time.perf_counter() - start) * 1000
            time.sleep(.5)
            before = usage(process.pid)
            time.sleep(1)
            after = usage(process.pid)
            result['idle_cpu_s_over_1s'] = after['cpu_s'] - before['cpu_s']
            if args.page_turns:
                turns = measure_page_turns(instance, process.pid, range(1, args.page_turns + 1))
                # Give the one-second cache collector time to expire old responses.
                time.sleep(3)
                settled = usage(process.pid)
                revisits = measure_page_turns(instance, process.pid, range(min(5, args.page_turns)))
                result['page_traversal'] = {'before': after, 'turns': turns,
                                            'settled': settled, 'revisits': revisits}
                command(instance, 'set_view_state', '0 500 1.5 0')
            if args.rapid_jumps:
                max_jump_page = max(1, min(31, args.page_turns if args.page_turns else 31))
                dest_count = max(1, args.rapid_destinations)
                destinations = [(max_jump_page * (d + 1)) // (dest_count + 1) for d in range(dest_count)]
                dest_trials = []
                for d, dest_page in enumerate(destinations):
                    stride = 7 + d * 2
                    seq = [((i * stride + 3 + d * 5) % max_jump_page) for i in range(args.rapid_jumps - 1)]
                    for idx in range(len(seq)):
                        if seq[idx] == dest_page:
                            seq[idx] = (seq[idx] + 3) % max_jump_page
                    seq.append(dest_page)
                    rapid_res = measure_rapid_jumps(instance, process.pid, seq)
                    time.sleep(1)
                    isolated_wait = measure_isolated_jump(instance, process.pid, 0, dest_page)
                    rapid_res['isolated_render_wait_ms'] = isolated_wait
                    rapid_res['queue_delay_ms'] = rapid_res['final_render_wait_ms'] - isolated_wait
                    dest_trials.append(rapid_res)
                    time.sleep(0.5)
                result['rapid_navigation'] = dest_trials
                command(instance, 'set_view_state', '0 500 1.5 0')
            modes = [('normal', None)] if args.normal_only else [
                ('normal', None), ('dark', 'toggle_dark_mode'), ('custom', 'toggle_custom_color')]
            for mode, toggle in modes:
                if toggle:
                    command(instance, toggle)
                command(instance, 'wait_for_renders_to_finish')
                time.sleep(.25)
                before = usage(process.pid)
                times = []
                view_checks = []
                begin = time.perf_counter()
                late = 0
                for frame in range(args.frames):
                    scheduled = begin + frame / args.fps
                    remaining = scheduled - time.perf_counter()
                    if remaining > 0:
                        time.sleep(remaining)
                    elif -remaining > 1 / args.fps:
                        late += 1
                    offset = 500 + 160 * math.sin(2 * math.pi * frame / args.frames)
                    request = time.perf_counter()
                    command(instance, 'set_view_state', f'0 {offset:.4f} 1.5 0')
                    times.append((time.perf_counter() - request) * 1000)
                    if frame % args.fps == 0 or frame == args.frames - 1:
                        view_checks.append({'frame': frame, 'state': checked_view(instance, offset)})
                remaining = begin + args.frames / args.fps - time.perf_counter()
                if remaining > 0:
                    time.sleep(remaining)
                elapsed = time.perf_counter() - begin
                after = usage(process.pid)
                command(instance, 'wait_for_renders_to_finish')
                cpu_seconds = after['cpu_s'] - before['cpu_s']
                # Readback+PNG is timed separately, with no new PDF rasterization.
                command(instance, 'set_view_state', '0 500 1.5 0')
                command(instance, 'wait_for_renders_to_finish')
                path = home / (mode + '.png')
                for _ in range(3):
                    command(instance, 'framebuffer_screenshot', path)
                captures = []
                for _ in range(args.captures):
                    tick = time.perf_counter()
                    command(instance, 'framebuffer_screenshot', path)
                    captures.append((time.perf_counter() - tick) * 1000)
                settled_view = checked_view(instance, 500)
                result['samples'][mode] = {
                    'scroll_cpu_s': cpu_seconds, 'scroll_wall_s': elapsed,
                    'scroll_cpu_percent_one_core': 100 * cpu_seconds / elapsed,
                    'scroll_request_ms': times, 'late_schedule_count': late,
                    'capture_ms': captures, 'rss_mib': after['rss_mib'],
                    'peak_rss_mib': usage(process.pid)['peak_rss_mib'],
                    'view_checks': view_checks, 'settled_view': settled_view}
                if png_dimensions(path) != result['framebuffer']:
                    raise RuntimeError('Framebuffer resized during benchmark')
            command(instance, 'quit')
            if process.wait(timeout=15) != 0:
                raise RuntimeError(f'{backend} did not exit cleanly')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=15)
    check_reader_log(home / 'reader.log', validation=args.validation)
    return result


def main():
    args = arguments()
    args.output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='sioyek-perf-', dir=args.output.resolve()))
    binaries = ({'baseline': args.baseline.resolve()}
                if args.baseline else {'opengl': args.opengl.resolve(), 'vulkan': args.vulkan.resolve()})
    if args.candidate:
        binaries['candidate'] = args.candidate.resolve()
    previous = None
    if shutil.which('hyprctl') and os.environ.get('HYPRLAND_INSTANCE_SIGNATURE'):
        previous = json.loads(hypr('-j', 'activewindow')).get('address')
    data = {'method': 'Paced IPC scrolling + CPU/RSS; synchronous framebuffer readback + PNG, not FPS',
            'pdf_sha256': hashlib.sha256(args.pdf.read_bytes()).hexdigest(),
            'binaries': {name: {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                         for name, path in binaries.items()},
            'rounds': args.rounds, 'frames_per_mode': args.frames, 'target_updates_per_second': args.fps,
            'captures_per_mode': args.captures, 'platform': args.platform,
            'page_turns': args.page_turns,
            'rapid_jumps': args.rapid_jumps,
            'rapid_destinations': args.rapid_destinations,
            'vulkan_validation': args.validation, 'trials': []}
    data['normal_only'] = args.normal_only
    try:
        for index in range(args.rounds):
            order = tuple(binaries) if index % 2 == 0 else tuple(reversed(binaries))
            for backend in order:
                result = trial(args, backend, binaries[backend], len(data['trials']), work)
                result['round'] = index + 1
                data['trials'].append(result)
                viewports = {tuple(t['viewport']) for t in data['trials']}
                if len(viewports) != 1:
                    raise RuntimeError(f'Unequal viewports invalidate comparison: {viewports}')
                if len({tuple(t['framebuffer']) for t in data['trials']}) != 1:
                    raise RuntimeError('Unequal framebuffer sizes invalidate comparison')
                (work / 'results.json').write_text(json.dumps(data, indent=2) + '\n')
                print(f'Completed {len(data["trials"])}/{args.rounds * len(binaries)}: {backend}, viewport={result["viewport"]}', flush=True)
    finally:
        if previous:
            try:
                restore_focus(previous)
            except subprocess.SubprocessError as error:
                print(f'Could not restore prior focus: {error}')
        print(f'Benchmark evidence: {work}', flush=True)
    for mode in (('normal',) if args.normal_only else ('normal', 'dark', 'custom')):
        summary = {}
        for backend in binaries:
            samples = [t['samples'][mode] for t in data['trials'] if t['backend'] == backend]
            summary[backend] = {
                'capture_median_ms': statistics.median(v for sample in samples for v in sample['capture_ms']),
                'scroll_cpu_median_percent_one_core': statistics.median(s['scroll_cpu_percent_one_core'] for s in samples),
                'rss_median_mib': statistics.median(s['rss_mib'] for s in samples)}
        print(mode, json.dumps(summary), flush=True)

    if args.rapid_jumps:
        for backend in binaries:
            rapid_trials = [t['rapid_navigation'] for t in data['trials'] if t['backend'] == backend and 'rapid_navigation' in t]
            flat_samples = [s for trial_list in rapid_trials for s in trial_list]
            if flat_samples:
                final_waits = [s['final_render_wait_ms'] for s in flat_samples]
                isolated_waits = [s['isolated_render_wait_ms'] for s in flat_samples]
                queue_delays = [s['queue_delay_ms'] for s in flat_samples]
                destinations = sorted(list({s['final_page'] for s in flat_samples}))
                print('rapid_navigation', backend, json.dumps({
                    'destinations': destinations,
                    'sample_count': len(flat_samples),
                    'final_wait_median_ms': statistics.median(final_waits),
                    'final_wait_p95_ms': percentile_95(final_waits),
                    'isolated_wait_median_ms': statistics.median(isolated_waits),
                    'isolated_wait_p95_ms': percentile_95(isolated_waits),
                    'queue_delay_median_ms': statistics.median(queue_delays),
                    'queue_delay_p95_ms': percentile_95(queue_delays),
                }), flush=True)


if __name__ == '__main__':
    main()
