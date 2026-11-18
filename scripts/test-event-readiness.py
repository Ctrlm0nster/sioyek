#!/usr/bin/env python3
"""Comprehensive benchmark for event-driven render readiness vs baseline polling.

Tests:
1. Cached pages (instant return without polling)
2. Uncached jumps (event-driven response as soon as raster completes)
3. Active background search contention
4. Mixed-width overview (both zoom directions)

Measures:
- Last required raster completion -> IPC reply
- Navigation -> Readiness Reply
- Nanosecond timestamps and millisecond latencies
- Frame production vs polling wait breakdown
- Immediate framebuffer screenshot verification
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib


def wire_arguments(values):
    encoded = [value.encode('utf-16-be') for value in values]
    result = struct.pack('>I', len(encoded))
    result += b''.join(struct.pack('>I', len(value)) + value for value in encoded)
    if len(result) >= 4096:
        raise ValueError('IPC request exceeds packet limit')
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


class SyntheticPDF:
    def __init__(self):
        self.objects = [b'', b'']
        self.pages = []
        self.font = self.add(b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>')

    def add(self, data):
        self.objects.append(data)
        return len(self.objects)

    def stream(self, data):
        packed = zlib.compress(data, 6)
        return self.add(b'<< /Filter /FlateDecode /Length ' + str(len(packed)).encode() +
                        b' >>\nstream\n' + packed + b'\nendstream')

    def page(self, width, height, text_content):
        content = (f"BT /F1 24 Tf 50 {height - 100} Td ({text_content}) Tj ET\n"
                   f"BT /F1 16 Tf 50 {height - 150} Td (Page size: {width}x{height}) Tj ET\n").encode()
        stream = self.stream(content)
        page = self.add(f'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {width} {height}] /Contents {stream} 0 R '
                        f'/Resources << /Font << /F1 {self.font} 0 R >> >> >>'.encode())
        self.pages.append(page)

    def write(self, path):
        self.objects[0] = b'<< /Type /Catalog /Pages 2 0 R >>'
        refs = b' '.join(f'{i} 0 R'.encode() for i in self.pages)
        self.objects[1] = f'<< /Type /Pages /Count {len(self.pages)} /Kids ['.encode() + refs + b'] >>'
        output = bytearray(b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n')
        offsets = [0]
        for index, obj in enumerate(self.objects, 1):
            offsets.append(len(output))
            output.extend(f'{index} 0 obj\n'.encode() + obj + b'\nendobj\n')
        start = len(output)
        output.extend(f'xref\n0 {len(offsets)}\n0000000000 65535 f \n'.encode())
        for offset in offsets[1:]:
            output.extend(f'{offset:010d} 00000 n \n'.encode())
        output.extend(f'trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{start}\n%%EOF\n'.encode())
        path.write_bytes(output)


def parse_diag_events(log_text):
    """Parses timing diag entries into chronological events."""
    events = []
    for line in log_text.splitlines():
        if '[TIMING_DIAG]' not in line:
            continue
        m = re.search(r'\[TIMING_DIAG\]\s+(\w+)\s*(.*)', line)
        if not m:
            continue
        tag = m.group(1)
        rest = m.group(2)
        kv = {}
        for k, v in re.findall(r'(\w+)=((?:\{.*?\})|[^\s]+)', rest):
            kv[k] = v
        # Extract nanosecond timestamp, fallback to us*1000
        t_ns = int(kv['t_ns']) if 't_ns' in kv else int(kv.get('t_us', 0)) * 1000
        events.append({'tag': tag, 't_ns': t_ns, 'kv': kv, 'raw': line})
    return events


def run_benchmark(binary_path, label):
    print(f'\n========================================', flush=True)
    print(f'Running Benchmark on [{label}]: {binary_path}', flush=True)
    print(f'========================================', flush=True)

    work = Path(tempfile.mkdtemp(prefix=f'sioyek-bench-{label}-'))
    pdf_path = work / 'test_document.pdf'

    # Build a 30-page mixed PDF:
    # Pages 0-4: Base size 600x800
    # Page 5: 900x800 (wide)
    # Page 6: 400x800 (narrow)
    # Pages 7-19: Base size 600x800
    # Page 20: 1200x800 (ultra-wide)
    # Pages 21-29: Base size 600x800
    pdf = SyntheticPDF()
    for i in range(30):
        w = 600
        if i == 5:
            w = 900
        elif i == 6:
            w = 400
        elif i == 20:
            w = 1200
        pdf.page(w, 800, f"Section {i}: Technical content keyword_alpha_{i} search_target_{i%5}")
    pdf.write(pdf_path)

    config_dir = work / '.config/sioyek'
    config_dir.mkdir(parents=True)
    (config_dir / 'prefs_user.config').write_text(
        'uniform_page_widths 1\nsuper_fast_search 0\nautomatically_download_matching_paper_name 0\n'
    )

    env = dict(os.environ,
               HOME=str(work),
               XDG_CONFIG_HOME=str(work / '.config'),
               XDG_CONFIG_DIRS=str(work / 'empty'),
               XDG_DATA_HOME=str(work / '.local/share'),
               XDG_CACHE_HOME=str(work / '.cache'),
               QT_QPA_PLATFORM='wayland',
               QT_FORCE_STDERR_LOGGING='1',
               SIOYEK_TIMING_DIAG='1')
    if 'XDG_RUNTIME_DIR' not in env and Path('/run/user/1000').is_dir():
        env['XDG_RUNTIME_DIR'] = '/run/user/1000'
    if 'WAYLAND_DISPLAY' not in env:
        env['WAYLAND_DISPLAY'] = 'wayland-1'

    instance = f'bench-{label}-{os.getpid()}'
    log_path = work / 'reader.log'

    results = {}

    with log_path.open('w') as log_file:
        proc = subprocess.Popen([str(binary_path), '--instance-name', instance,
                                 '--no-auto-config', '--nofocus', str(pdf_path.resolve())],
                                env=env, stdout=log_file, stderr=log_file)
        try:
            wait_for_reader_startup(proc, instance)
            command(instance, 'toggle_fullscreen')
            time.sleep(0.15)
            command(instance, 'wait_for_renders_to_finish')

            # -------------------------------------------------------------
            # Scenario 1: Uncached Page Jump (Page 0 -> Page 10)
            # -------------------------------------------------------------
            print("\n--- Scenario 1: Uncached Jump (Page 0 -> Page 10) ---", flush=True)
            t_nav0 = time.perf_counter()
            command(instance, 'set_state_json', json.dumps({'page_number': 10}))
            t_wait_start = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            t_reply = time.perf_counter()
            uncached_nav_ms = (t_reply - t_nav0) * 1000
            uncached_wait_ms = (t_reply - t_wait_start) * 1000
            # Framebuffer capture
            fb1 = work / 'uncached_p10.png'
            command(instance, 'framebuffer_screenshot', str(fb1))
            fb1_hash = hashlib.sha256(fb1.read_bytes()).hexdigest()[:12] if fb1.is_file() else "FAIL"
            print(f"[{label}] Uncached Jump: nav_to_reply={uncached_nav_ms:.2f} ms, wait_cmd={uncached_wait_ms:.2f} ms, fb_sha={fb1_hash}", flush=True)

            # -------------------------------------------------------------
            # Scenario 2: Cached Page Return (Page 10 -> Page 0)
            # -------------------------------------------------------------
            print("\n--- Scenario 2: Cached Page Return (Page 10 -> Page 0) ---", flush=True)
            t_nav0 = time.perf_counter()
            command(instance, 'set_state_json', json.dumps({'page_number': 0}))
            t_wait_start = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            t_reply = time.perf_counter()
            cached_nav_ms = (t_reply - t_nav0) * 1000
            cached_wait_ms = (t_reply - t_wait_start) * 1000
            fb2 = work / 'cached_p0.png'
            command(instance, 'framebuffer_screenshot', str(fb2))
            fb2_hash = hashlib.sha256(fb2.read_bytes()).hexdigest()[:12] if fb2.is_file() else "FAIL"
            print(f"[{label}] Cached Return: nav_to_reply={cached_nav_ms:.2f} ms, wait_cmd={cached_wait_ms:.2f} ms, fb_sha={fb2_hash}", flush=True)

            # -------------------------------------------------------------
            # Scenario 3: Active Search Contention Jump (Jump to Page 15 while searching)
            # -------------------------------------------------------------
            print("\n--- Scenario 3: Active Search Contention (Page 0 -> Page 15) ---", flush=True)
            command(instance, 'search', 'keyword_alpha')
            time.sleep(0.01)
            t_nav0 = time.perf_counter()
            command(instance, 'set_state_json', json.dumps({'page_number': 15}))
            t_wait_start = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            t_reply = time.perf_counter()
            search_nav_ms = (t_reply - t_nav0) * 1000
            search_wait_ms = (t_reply - t_wait_start) * 1000
            fb3 = work / 'search_p15.png'
            command(instance, 'framebuffer_screenshot', str(fb3))
            fb3_hash = hashlib.sha256(fb3.read_bytes()).hexdigest()[:12] if fb3.is_file() else "FAIL"
            command(instance, 'wait_for_search_to_finish')
            command(instance, 'escape')
            print(f"[{label}] Active Search Jump: nav_to_reply={search_nav_ms:.2f} ms, wait_cmd={search_wait_ms:.2f} ms, fb_sha={fb3_hash}", flush=True)

            # -------------------------------------------------------------
            # Scenario 4: Mixed-Width Overview (Pages 5 [wide] & 6 [narrow])
            # -------------------------------------------------------------
            print("\n--- Scenario 4: Mixed-Width Overview ---", flush=True)
            # Step A: Wide overview
            command(instance, 'search', 'keyword_alpha_5')
            time.sleep(0.05)
            command(instance, 'wait_for_search_to_finish')
            t_nav0 = time.perf_counter()
            command(instance, 'overview_next_item')
            t_wait_start = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            t_reply = time.perf_counter()
            ov_wide_nav_ms = (t_reply - t_nav0) * 1000
            ov_wide_wait_ms = (t_reply - t_wait_start) * 1000
            fb_ov1 = work / 'ov_wide_p5.png'
            command(instance, 'framebuffer_screenshot', str(fb_ov1))
            fb_ov1_hash = hashlib.sha256(fb_ov1.read_bytes()).hexdigest()[:12] if fb_ov1.is_file() else "FAIL"
            print(f"[{label}] Wide Overview (Page 5, w=900): nav_to_reply={ov_wide_nav_ms:.2f} ms, wait_cmd={ov_wide_wait_ms:.2f} ms, fb_sha={fb_ov1_hash}", flush=True)

            # Step B: Narrow overview
            command(instance, 'close_overview')
            command(instance, 'escape')
            command(instance, 'search', 'keyword_alpha_6')
            time.sleep(0.05)
            command(instance, 'wait_for_search_to_finish')
            t_nav0 = time.perf_counter()
            command(instance, 'overview_next_item')
            t_wait_start = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            t_reply = time.perf_counter()
            ov_narr_nav_ms = (t_reply - t_nav0) * 1000
            ov_narr_wait_ms = (t_reply - t_wait_start) * 1000
            fb_ov2 = work / 'ov_narr_p6.png'
            command(instance, 'framebuffer_screenshot', str(fb_ov2))
            fb_ov2_hash = hashlib.sha256(fb_ov2.read_bytes()).hexdigest()[:12] if fb_ov2.is_file() else "FAIL"
            print(f"[{label}] Narrow Overview (Page 6, w=400): nav_to_reply={ov_narr_nav_ms:.2f} ms, wait_cmd={ov_narr_wait_ms:.2f} ms, fb_sha={fb_ov2_hash}", flush=True)

            command(instance, 'close_overview')
            command(instance, 'quit')
            proc.wait(timeout=5)

            results = {
                'uncached': {'nav_ms': uncached_nav_ms, 'wait_ms': uncached_wait_ms, 'fb_sha': fb1_hash},
                'cached': {'nav_ms': cached_nav_ms, 'wait_ms': cached_wait_ms, 'fb_sha': fb2_hash},
                'search': {'nav_ms': search_nav_ms, 'wait_ms': search_wait_ms, 'fb_sha': fb3_hash},
                'overview_wide': {'nav_ms': ov_wide_nav_ms, 'wait_ms': ov_wide_wait_ms, 'fb_sha': fb_ov1_hash},
                'overview_narrow': {'nav_ms': ov_narr_nav_ms, 'wait_ms': ov_narr_wait_ms, 'fb_sha': fb_ov2_hash},
                'log': log_path.read_text()
            }
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=5)

    return results


def analyze_log(log_text, label):
    """Extracts internal timing breakdown from TIMING_DIAG entries."""
    events = parse_diag_events(log_text)
    print(f"\n--- Internal Timing Diagnostic Breakdown for [{label}] ---", flush=True)
    # Find all pairs of wait_for_renders_start and ipc_reply
    wait_starts = [e for e in events if e['tag'] == 'wait_for_renders_start']
    ipc_replies = [e for e in events if e['tag'] == 'ipc_reply']
    rasters = [e for e in events if e['tag'] == 'raster_done']

    print(f"Total ipc_reply events logged: {len(ipc_replies)}")
    records = []
    for reply in ipc_replies:
        rep_ns = reply['t_ns']
        poll_ticks = int(reply['kv'].get('poll_ticks', 0))
        wait_ms = float(reply['kv'].get('wait_ms', 0.0))

        # Find the immediately preceding wait_for_renders_start
        prec_starts = [s for s in wait_starts if s['t_ns'] <= rep_ns]
        start_ns = prec_starts[-1]['t_ns'] if prec_starts else rep_ns

        # Find all preceding raster_dones
        prec_rasters = [r for r in rasters if r['t_ns'] <= rep_ns]
        last_raster = prec_rasters[-1] if prec_rasters else None
        last_raster_ns = last_raster['t_ns'] if last_raster else start_ns

        raster_to_reply_ms = (rep_ns - last_raster_ns) / 1000000.0 if last_raster else 0.0
        records.append({
            'reply_ns': rep_ns,
            'poll_ticks': poll_ticks,
            'wait_ms': wait_ms,
            'raster_to_reply_ms': raster_to_reply_ms,
            'last_raster_page': last_raster['kv'].get('page') if last_raster else None,
            'last_raster_ms': float(last_raster['kv'].get('raster_ms', 0.0)) if last_raster else 0.0
        })
        print(f"  Event: poll_ticks={poll_ticks}, wait_ms={wait_ms:.3f} ms, "
              f"last_raster_done->reply={raster_to_reply_ms:.3f} ms "
              f"(last raster page={records[-1]['last_raster_page']} took {records[-1]['last_raster_ms']:.2f} ms)")
    return records


def main():
    parser = argparse.ArgumentParser(description='Benchmark event-driven render readiness vs baseline/parent')
    parser.add_argument('baseline_bin', nargs='?', default='/usr/bin/sioyek', help='Baseline/parent binary path')
    parser.add_argument('candidate_bin', nargs='?', default='build/dev/sioyek', help='Candidate binary path')
    parser.add_argument('--baseline-label', default='Baseline', help='Label for baseline/parent')
    parser.add_argument('--candidate-label', default='Candidate', help='Label for candidate')
    args = parser.parse_args()

    baseline_bin = Path(args.baseline_bin)
    candidate_bin = Path(args.candidate_bin)
    base_lbl = args.baseline_label
    cand_lbl = args.candidate_label

    res_base = run_benchmark(baseline_bin, base_lbl)
    diag_base = analyze_log(res_base['log'], base_lbl)

    res_cand = run_benchmark(candidate_bin, cand_lbl)
    diag_cand = analyze_log(res_cand['log'], cand_lbl)

    print("\n" + "="*80)
    print(f"COMPARATIVE SUMMARY TABLE: {base_lbl.upper()} vs {cand_lbl.upper()}")
    print("="*80)
    print(f"{'Scenario':<25} | {'Metric':<30} | {base_lbl:<15} | {cand_lbl:<15} | {'Improvement'}")
    print("-" * 105)

    scenarios = [
        ('1. Cached Page Return', 'cached'),
        ('2. Uncached Jump (P10)', 'uncached'),
        ('3. Active Search Contention', 'search'),
        ('4. Mixed-Width Ov (Wide)', 'overview_wide'),
        ('5. Mixed-Width Ov (Narrow)', 'overview_narrow'),
    ]

    for title, key in scenarios:
        b_nav = res_base[key]['nav_ms']
        c_nav = res_cand[key]['nav_ms']
        diff_nav = b_nav - c_nav
        print(f"{title:<25} | {'Navigation -> Readiness Reply':<30} | {b_nav:>11.2f} ms   | {c_nav:>11.2f} ms   | {diff_nav:+8.2f} ms ({diff_nav/b_nav*100:+.1f}%)")

        b_wait = res_base[key]['wait_ms']
        c_wait = res_cand[key]['wait_ms']
        diff_wait = b_wait - c_wait
        print(f"{'':<25} | {'wait_for_renders wait':<30} | {b_wait:>11.2f} ms   | {c_wait:>11.2f} ms   | {diff_wait:+8.2f} ms ({diff_wait/b_wait*100:+.1f}%)")
        print("-" * 105)

    print("\n" + "="*80)
    print("INTERNAL DIAGNOSTICS: Last Required Raster -> IPC Reply & Polling")
    print("="*80)
    print(f"{base_lbl} Polling Latencies:")
    for i, d in enumerate(diag_base):
        print(f"  Step {i+1}: poll_ticks={d['poll_ticks']}, wait_ms={d['wait_ms']:.3f} ms, raster->reply={d['raster_to_reply_ms']:.3f} ms")
    print(f"{cand_lbl} Event-Driven Latencies:")
    for i, d in enumerate(diag_cand):
        print(f"  Step {i+1}: poll_ticks={d['poll_ticks']}, wait_ms={d['wait_ms']:.3f} ms, raster->reply={d['raster_to_reply_ms']:.3f} ms")


if __name__ == '__main__':
    main()
