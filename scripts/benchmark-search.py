#!/usr/bin/env python3
"""Benchmark search responsiveness, background indexing, and render contention.

Measures:
1. Background indexing time and resource usage immediately after opening large documents.
2. Search responsiveness:
   - Unindexed time-to-first-match and total search time (page-by-page traversal).
   - Indexed in-memory super-fast search time and match counts.
3. Render responsiveness under search contention:
   - Page turn render settle latency during reader idle.
   - Page turn render settle latency while background search traverses the document.
   - Contention penalty (delta).
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time


def wire_arguments(values):
    encoded = [value.encode('utf-16-be') for value in values]
    result = struct.pack('>I', len(encoded))
    result += b''.join(struct.pack('>I', len(value)) + value for value in encoded)
    if len(result) >= 4096:
        raise ValueError('IPC request exceeds benchmark packet limit')
    return result


def command(instance, name, data=None, timeout=45):
    values = ['sioyek', '--instance-name', instance, '--nofocus',
              '--execute-command', name, '--wait-for-response']
    if data is not None:
        values += ['--execute-command-data', str(data)]
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as channel:
        channel.settimeout(timeout)
        channel.connect(str(Path(tempfile.gettempdir()) / instance))
        channel.sendall(wire_arguments(values))
        response = b''
        while len(response) < 1024 * 1024:
            part = channel.recv(65536)
            if not part:
                if name in ('quit', 'close_window'):
                    return None
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
    fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    cpu = (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
    status = dict(line.split(':', 1) for line in Path(f'/proc/{pid}/status').read_text().splitlines())
    return {'cpu_s': cpu, 'rss_mib': int(status['VmRSS'].split()[0]) / 1024,
            'peak_rss_mib': int(status['VmHWM'].split()[0]) / 1024}


def hypr(*args):
    return subprocess.check_output(['hyprctl', *args], stderr=subprocess.DEVNULL).decode('utf-8')


def restore_focus(address):
    try:
        hypr('dispatch', 'focuswindow', f'address:{address}')
    except Exception:
        pass


def percentile_95(values):
    if not values:
        return 0.0
    sorted_v = sorted(values)
    k = (len(sorted_v) - 1) * 0.95
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return sorted_v[int(k)]
    return sorted_v[f] * (c - k) + sorted_v[c] * (k - f)


def median(values):
    if not values:
        return 0.0
    sorted_v = sorted(values)
    n = len(sorted_v)
    if n % 2 == 1:
        return sorted_v[n // 2]
    return (sorted_v[n // 2 - 1] + sorted_v[n // 2]) / 2.0


def wait_for_reader_startup(proc, instance, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError('Reader exited prematurely during startup')
        try:
            state = command(instance, 'get_state_json', timeout=2)[0]
            if state.get('document_path'):
                return state
        except Exception:
            pass
        time.sleep(0.015)
    raise TimeoutError('Reader startup timed out')


def test_indexing_and_indexed_search(binary, pdf, queries, args, work, label, trial_idx):
    """Measures startup, background indexing time, and indexed search speed."""
    home = work / f'{trial_idx:02d}-{label}-indexed'
    config = home / '.config/sioyek'
    config.mkdir(parents=True)
    (config / 'prefs_user.config').write_text(
        'super_fast_search 1\nautomatically_download_matching_paper_name 0\n')
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
               XDG_CONFIG_DIRS=str(home / 'empty'), XDG_DATA_HOME=str(home / '.local/share'),
               XDG_CACHE_HOME=str(home / '.cache'), QT_QPA_PLATFORM=args.platform,
               QT_FORCE_STDERR_LOGGING='1',
               SIOYEK_VULKAN_VALIDATION='1' if args.validation else '0')
    if os.environ.get('SIOYEK_TEST_LIBRARY_PATH'):
        env['LD_LIBRARY_PATH'] = os.environ['SIOYEK_TEST_LIBRARY_PATH']

    instance = f'sioyek-idx-{os.getpid()}-{trial_idx}'
    result = {'label': label, 'pdf': pdf.name, 'mode': 'indexed', 'queries': {}}

    with (home / 'reader.log').open('w') as log:
        t0 = time.perf_counter()
        proc = subprocess.Popen([str(binary), '--instance-name', instance, '--no-auto-config',
                                 '--nofocus', str(pdf.resolve())],
                                env=env, stdout=log, stderr=log)
        try:
            wait_for_reader_startup(proc, instance)
            result['startup_ms'] = (time.perf_counter() - t0) * 1000

            # Measure indexing time
            t_idx0 = time.perf_counter()
            command(instance, 'wait_for_indexing_to_finish', timeout=60)
            result['indexing_ms'] = (time.perf_counter() - t_idx0) * 1000
            result['post_index_usage'] = usage(proc.pid)

            # Map window and ensure render settled
            command(instance, 'toggle_fullscreen')
            time.sleep(0.2)
            command(instance, 'set_view_state', '0 500 1.5 0')
            command(instance, 'wait_for_renders_to_finish')

            # Measure indexed search for each query
            for q in queries:
                t_q0 = time.perf_counter()
                command(instance, 'search', q)
                command(instance, 'wait_for_search_to_finish')
                search_ms = (time.perf_counter() - t_q0) * 1000
                st = command(instance, 'get_state_json')[0]
                result['queries'][q] = {
                    'search_ms': search_ms,
                    'num_results': st.get('num_search_results', 0)
                }

            command(instance, 'quit')
            proc.wait(timeout=10)
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)

    return result


def test_unindexed_search(binary, pdf, queries, args, work, label, trial_idx):
    """Measures unindexed time to first search match and total page-traversal search time."""
    home = work / f'{trial_idx:02d}-{label}-unindexed'
    config = home / '.config/sioyek'
    config.mkdir(parents=True)
    (config / 'prefs_user.config').write_text(
        'super_fast_search 0\nautomatically_download_matching_paper_name 0\n')
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
               XDG_CONFIG_DIRS=str(home / 'empty'), XDG_DATA_HOME=str(home / '.local/share'),
               XDG_CACHE_HOME=str(home / '.cache'), QT_QPA_PLATFORM=args.platform,
               QT_FORCE_STDERR_LOGGING='1',
               SIOYEK_VULKAN_VALIDATION='1' if args.validation else '0')
    if os.environ.get('SIOYEK_TEST_LIBRARY_PATH'):
        env['LD_LIBRARY_PATH'] = os.environ['SIOYEK_TEST_LIBRARY_PATH']

    instance = f'sioyek-unidx-{os.getpid()}-{trial_idx}'
    result = {'label': label, 'pdf': pdf.name, 'mode': 'unindexed', 'queries': {}}

    with (home / 'reader.log').open('w') as log:
        proc = subprocess.Popen([str(binary), '--instance-name', instance, '--no-auto-config',
                                 '--nofocus', str(pdf.resolve())],
                                env=env, stdout=log, stderr=log)
        try:
            wait_for_reader_startup(proc, instance)
            command(instance, 'toggle_fullscreen')
            time.sleep(0.2)
            command(instance, 'set_view_state', '0 500 1.5 0')
            command(instance, 'wait_for_renders_to_finish')

            for q in queries:
                t_q0 = time.perf_counter()
                command(instance, 'search', q)
                first_match_ms = None
                poll_deadline = time.monotonic() + 30
                while time.monotonic() < poll_deadline:
                    st = command(instance, 'get_state_json')[0]
                    cnt = st.get('num_search_results', 0)
                    if cnt > 0 and first_match_ms is None:
                        first_match_ms = (time.perf_counter() - t_q0) * 1000
                        break
                    time.sleep(0.003)

                command(instance, 'wait_for_search_to_finish', timeout=60)
                total_search_ms = (time.perf_counter() - t_q0) * 1000
                st = command(instance, 'get_state_json')[0]
                result['queries'][q] = {
                    'first_match_ms': first_match_ms if first_match_ms is not None else total_search_ms,
                    'total_search_ms': total_search_ms,
                    'num_results': st.get('num_search_results', 0)
                }
                # Reset search state before next query
                command(instance, 'escape')
                time.sleep(0.05)

            command(instance, 'quit')
            proc.wait(timeout=10)
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)

    return result


def test_contention(binary, pdf, turn_pages, background_query, args, work, label, trial_idx):
    """Measures page-turn render settle latency during idle vs during active background search."""
    home = work / f'{trial_idx:02d}-{label}-contention'
    config = home / '.config/sioyek'
    config.mkdir(parents=True)
    (config / 'prefs_user.config').write_text(
        'super_fast_search 0\nautomatically_download_matching_paper_name 0\n')
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
               XDG_CONFIG_DIRS=str(home / 'empty'), XDG_DATA_HOME=str(home / '.local/share'),
               XDG_CACHE_HOME=str(home / '.cache'), QT_QPA_PLATFORM=args.platform,
               QT_FORCE_STDERR_LOGGING='1',
               SIOYEK_VULKAN_VALIDATION='1' if args.validation else '0')
    if os.environ.get('SIOYEK_TEST_LIBRARY_PATH'):
        env['LD_LIBRARY_PATH'] = os.environ['SIOYEK_TEST_LIBRARY_PATH']

    instance = f'sioyek-contention-{os.getpid()}-{trial_idx}'
    result = {'label': label, 'pdf': pdf.name, 'idle_waits_ms': [], 'during_search_waits_ms': []}

    with (home / 'reader.log').open('w') as log:
        proc = subprocess.Popen([str(binary), '--instance-name', instance, '--no-auto-config',
                                 '--nofocus', str(pdf.resolve())],
                                env=env, stdout=log, stderr=log)
        try:
            wait_for_reader_startup(proc, instance)
            command(instance, 'toggle_fullscreen')
            time.sleep(0.2)
            command(instance, 'set_view_state', '0 500 1.5 0')
            command(instance, 'wait_for_renders_to_finish')

            # 1. Idle page turns baseline
            for page in turn_pages:
                t0 = time.perf_counter()
                command(instance, 'set_view_state', f'{page} 0 1.5 0')
                command(instance, 'wait_for_renders_to_finish')
                dt = (time.perf_counter() - t0) * 1000
                result['idle_waits_ms'].append(dt)

            # 2. Page turns during active search traversal
            # Offset destination pages slightly so we don't hit cached rendered pixmaps
            dest_pages = [p + 2 for p in turn_pages]
            for page in dest_pages:
                # Trigger search that scans through pages
                command(instance, 'search', background_query)
                t0 = time.perf_counter()
                # Turn page immediately while search is processing
                command(instance, 'set_view_state', f'{page} 0 1.5 0')
                command(instance, 'wait_for_renders_to_finish')
                dt = (time.perf_counter() - t0) * 1000
                result['during_search_waits_ms'].append(dt)
                command(instance, 'wait_for_search_to_finish')
                command(instance, 'escape')

            result['idle_median_ms'] = median(result['idle_waits_ms'])
            result['during_search_median_ms'] = median(result['during_search_waits_ms'])
            result['contention_delta_median_ms'] = result['during_search_median_ms'] - result['idle_median_ms']
            result['idle_p95_ms'] = percentile_95(result['idle_waits_ms'])
            result['during_search_p95_ms'] = percentile_95(result['during_search_waits_ms'])
            result['contention_delta_p95_ms'] = result['during_search_p95_ms'] - result['idle_p95_ms']

            command(instance, 'quit')
            proc.wait(timeout=10)
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)

    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True, help='Path to baseline sioyek binary')
    parser.add_argument('--candidate', type=Path, help='Path to candidate sioyek binary')
    parser.add_argument('--text-pdf', type=Path, required=True, help='Path to long text-heavy PDF')
    parser.add_argument('--mixed-pdf', type=Path, required=True, help='Path to mixed text-and-image PDF')
    parser.add_argument('--rounds', type=int, default=3, help='Number of benchmark rounds')
    parser.add_argument('--output', type=Path, required=True, help='Output directory for benchmark results')
    parser.add_argument('--platform', choices=('wayland', 'xcb'), default='wayland')
    parser.add_argument('--validation', action='store_true', help='Enable Vulkan validation layers')
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='search-bench-', dir=args.output.resolve()))

    binaries = {'baseline': args.baseline.resolve()}
    if args.candidate:
        binaries['candidate'] = args.candidate.resolve()

    # Pre-select search terms and navigation destinations
    text_queries = ['matrix', 'asymptote']
    mixed_queries = ['growth', 'culture']
    text_turn_pages = [15, 30, 45, 60, 75]
    mixed_turn_pages = [10, 25, 40, 55, 70]
    background_scan_query = 'zznonexistenttermxyz'

    data = {
        'method': __doc__,
        'arguments': {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
        'binaries': {k: {'path': str(v), 'sha256': hashlib.sha256(v.read_bytes()).hexdigest()}
                     for k, v in binaries.items()},
        'documents': {
            'text': {'path': str(args.text_pdf.resolve()), 'sha256': hashlib.sha256(args.text_pdf.read_bytes()).hexdigest()},
            'mixed': {'path': str(args.mixed_pdf.resolve()), 'sha256': hashlib.sha256(args.mixed_pdf.read_bytes()).hexdigest()}
        },
        'results': []
    }

    previous_focus = None
    if shutil.which('hyprctl') and os.environ.get('HYPRLAND_INSTANCE_SIGNATURE'):
        try:
            previous_focus = json.loads(hypr('-j', 'activewindow')).get('address')
        except Exception:
            pass

    trial_counter = 0
    try:
        for round_idx in range(args.rounds):
            order = tuple(binaries) if round_idx % 2 == 0 else tuple(reversed(binaries))
            for label in order:
                bin_path = binaries[label]
                print(f'\n--- Round {round_idx + 1}/{args.rounds} [{label}] ---', flush=True)

                # 1. Text PDF: Indexing & Indexed search
                print(f'[{label}] Text PDF: Indexing & Indexed search...', flush=True)
                r_text_idx = test_indexing_and_indexed_search(
                    bin_path, args.text_pdf, text_queries, args, work, label, trial_counter)
                trial_counter += 1

                # 2. Text PDF: Unindexed search
                print(f'[{label}] Text PDF: Unindexed search...', flush=True)
                r_text_unidx = test_unindexed_search(
                    bin_path, args.text_pdf, text_queries, args, work, label, trial_counter)
                trial_counter += 1

                # 3. Text PDF: Render Contention
                print(f'[{label}] Text PDF: Render Contention test...', flush=True)
                r_text_cont = test_contention(
                    bin_path, args.text_pdf, text_turn_pages, background_scan_query, args, work, label, trial_counter)
                trial_counter += 1

                # 4. Mixed PDF: Indexing & Indexed search
                print(f'[{label}] Mixed PDF: Indexing & Indexed search...', flush=True)
                r_mixed_idx = test_indexing_and_indexed_search(
                    bin_path, args.mixed_pdf, mixed_queries, args, work, label, trial_counter)
                trial_counter += 1

                # 5. Mixed PDF: Unindexed search
                print(f'[{label}] Mixed PDF: Unindexed search...', flush=True)
                r_mixed_unidx = test_unindexed_search(
                    bin_path, args.mixed_pdf, mixed_queries, args, work, label, trial_counter)
                trial_counter += 1

                # 6. Mixed PDF: Render Contention
                print(f'[{label}] Mixed PDF: Render Contention test...', flush=True)
                r_mixed_cont = test_contention(
                    bin_path, args.mixed_pdf, mixed_turn_pages, background_scan_query, args, work, label, trial_counter)
                trial_counter += 1

                round_record = {
                    'round': round_idx,
                    'label': label,
                    'text_pdf': {
                        'indexing': r_text_idx,
                        'unindexed': r_text_unidx,
                        'contention': r_text_cont
                    },
                    'mixed_pdf': {
                        'indexing': r_mixed_idx,
                        'unindexed': r_mixed_unidx,
                        'contention': r_mixed_cont
                    }
                }
                data['results'].append(round_record)
                (work / 'results.json').write_text(json.dumps(data, indent=2) + '\n')

    finally:
        if previous_focus:
            restore_focus(previous_focus)

    print(f'\nBenchmark finished. Evidence directory: {work}', flush=True)
    # Generate final report
    summary_path = work / 'REPORT.md'
    write_summary_report(data, summary_path)
    print(f'Report written to {summary_path}\n')
    print(summary_path.read_text())


def write_summary_report(data, output_path):
    rounds = data['results']
    labels = sorted(list(set(r['label'] for r in rounds)))

    report = []
    report.append('# Large PDF Search & Contention Benchmark Report\n')
    report.append('Evaluation of background indexing time, search responsiveness (unindexed vs indexed), and render contention during active background traversal.\n')

    for doc_key, doc_name in [('text_pdf', 'Dense Text PDF (textbook_of_engineering_matematics.pdf, 447 pages)'),
                              ('mixed_pdf', 'Mixed Text & Image PDF (02.pdf, 171 pages)')]:
        report.append(f'## {doc_name}\n')

        # Indexing Table
        report.append('### Background Indexing & Indexed Search\n')
        report.append('| Reader | Indexing Wait (ms) | Peak RSS (MiB) | Query 1 Indexed (ms) | Query 2 Indexed (ms) |')
        report.append('| :--- | :---: | :---: | :---: | :---: |')
        for label in labels:
            lbl_rounds = [r[doc_key]['indexing'] for r in rounds if r['label'] == label]
            idx_ms = median([r['indexing_ms'] for r in lbl_rounds])
            rss = median([r['post_index_usage']['peak_rss_mib'] for r in lbl_rounds])
            q_names = list(lbl_rounds[0]['queries'].keys())
            q1_ms = median([r['queries'][q_names[0]]['search_ms'] for r in lbl_rounds])
            q2_ms = median([r['queries'][q_names[1]]['search_ms'] for r in lbl_rounds])
            report.append(f'| {label} | {idx_ms:.1f} | {rss:.1f} | {q1_ms:.1f} ("{q_names[0]}") | {q2_ms:.1f} ("{q_names[1]}") |')
        report.append('\n')

        # Unindexed Search Table
        report.append('### Unindexed Search (Page-by-Page Traversal)\n')
        report.append('| Reader | Query | First Match (ms) | Total Search (ms) | Matches |')
        report.append('| :--- | :--- | :---: | :---: | :---: |')
        for label in labels:
            lbl_rounds = [r[doc_key]['unindexed'] for r in rounds if r['label'] == label]
            q_names = list(lbl_rounds[0]['queries'].keys())
            for q in q_names:
                fm = median([r['queries'][q]['first_match_ms'] for r in lbl_rounds])
                ts = median([r['queries'][q]['total_search_ms'] for r in lbl_rounds])
                cnt = lbl_rounds[0]['queries'][q]['num_results']
                report.append(f'| {label} | "{q}" | {fm:.1f} | {ts:.1f} | {cnt} |')
        report.append('\n')

        # Contention Table
        report.append('### Render Contention (Page Turns During Background Search)\n')
        report.append('| Reader | Idle Median Wait | During Search Median | Contention Delta (Median) | Idle P95 | During Search P95 | Contention Delta (P95) |')
        report.append('| :--- | :---: | :---: | :---: | :---: | :---: | :---: |')
        for label in labels:
            lbl_rounds = [r[doc_key]['contention'] for r in rounds if r['label'] == label]
            idle_med = median([r['idle_median_ms'] for r in lbl_rounds])
            srch_med = median([r['during_search_median_ms'] for r in lbl_rounds])
            delta_med = median([r['contention_delta_median_ms'] for r in lbl_rounds])
            idle_p95 = median([r['idle_p95_ms'] for r in lbl_rounds])
            srch_p95 = median([r['during_search_p95_ms'] for r in lbl_rounds])
            delta_p95 = median([r['contention_delta_p95_ms'] for r in lbl_rounds])
            report.append(f'| {label} | {idle_med:.1f} ms | {srch_med:.1f} ms | {delta_med:+.1f} ms | {idle_p95:.1f} ms | {srch_p95:.1f} ms | {delta_p95:+.1f} ms |')
        report.append('\n')

    output_path.write_text('\n'.join(report) + '\n')


if __name__ == '__main__':
    main()
