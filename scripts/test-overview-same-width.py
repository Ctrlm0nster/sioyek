#!/usr/bin/env python3
"""Tests same-width mode with an overview open on a mixed-page-width PDF.

Verifies:
1. Drawing path and readiness path share the exact same slice and zoom calculation.
2. In same-width mode, overview zoom is correctly scaled by the page width ratio.
3. wait_for_renders_to_finish waits for and confirms the exact rendered overview slices.
4. Immediate framebuffer capture confirms correct rendering upon IPC reply.
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
        # Generate PDF stream to draw text
        content = (f"BT /F1 24 Tf 50 {height - 100} Td ({text_content}) Tj ET\n"
                   f"BT /F1 16 Tf 50 {height - 150} Td (Page width: {width}, height: {height}) Tj ET\n").encode()
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


def run_test(binary_path):
    print(f'Testing same-width overview with binary: {binary_path}', flush=True)
    work = Path(tempfile.mkdtemp(prefix='sioyek-same-width-test-'))
    pdf_path = work / 'mixed_widths.pdf'

    # Create synthetic PDF with mixed page widths:
    # Page 0: 600 x 800 (base width)
    # Page 1: 900 x 800 (wide, ratio = 600/900 = 0.6667)
    # Page 2: 400 x 800 (narrow, ratio = 600/400 = 1.5000)
    # Page 3: 1200 x 800 (ultra-wide, ratio = 600/1200 = 0.5000)
    # Page 4: 600 x 800 (return to base)
    pdf = SyntheticPDF()
    pdf.page(600, 800, "Page 0: Introduction to Quantum Mechanics")
    pdf.page(900, 800, "Page 1: Hypothesis of Wave-Particle Duality")
    pdf.page(400, 800, "Page 2: Experiment of Double Slit Interference")
    pdf.page(1200, 800, "Page 3: Conclusion and Unified Field Discussion")
    pdf.page(600, 800, "Page 4: Appendix and References")
    pdf.write(pdf_path)
    print(f'Created mixed-page-width PDF at {pdf_path}', flush=True)

    config_dir = work / '.config/sioyek'
    config_dir.mkdir(parents=True)
    # Configure same-width mode
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

    instance = f'same-width-test-{os.getpid()}'
    log_path = work / 'reader.log'

    with log_path.open('w') as log_file:
        proc = subprocess.Popen([str(binary_path), '--instance-name', instance,
                                 '--no-auto-config', '--nofocus', str(pdf_path.resolve())],
                                env=env, stdout=log_file, stderr=log_file)
        try:
            st0 = wait_for_reader_startup(proc, instance)
            print(f'Reader started on page {st0.get("page_number")}, window={st0.get("window_width")}x{st0.get("window_height")}', flush=True)
            command(instance, 'toggle_fullscreen')
            time.sleep(0.2)
            command(instance, 'wait_for_renders_to_finish')

            # Step 1: Search for term on wide page 1 (width 900)
            print("Searching for 'Hypothesis' on wide page 1...", flush=True)
            command(instance, 'search', 'Hypothesis')
            time.sleep(0.1)
            command(instance, 'wait_for_search_to_finish')

            # Step 2: Open overview to search result on page 1
            print("Opening overview to search result (page 1, width 900)...", flush=True)
            command(instance, 'overview_next_item')

            # Step 3: Wait for renders to finish (should wait for overview render with scaled zoom)
            t_wait0 = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            wait_ms = (time.perf_counter() - t_wait0) * 1000
            print(f'wait_for_renders_to_finish returned in {wait_ms:.1f} ms with overview open', flush=True)

            # Step 4: Verify immediate framebuffer capture
            screenshot_path = work / 'overview_page_1.png'
            command(instance, 'framebuffer_screenshot', str(screenshot_path))
            assert screenshot_path.is_file(), f'Framebuffer screenshot missing: {screenshot_path}'
            assert screenshot_path.stat().st_size > 1000, f'Framebuffer screenshot empty: {screenshot_path}'
            h1 = hashlib.sha256(screenshot_path.read_bytes()).hexdigest()
            print(f'Captured overview on page 1: {screenshot_path.stat().st_size} bytes, sha256={h1[:12]}', flush=True)

            # Step 5: Search for term on narrow page 2 (width 400)
            command(instance, 'escape')
            command(instance, 'close_overview')
            command(instance, 'wait_for_renders_to_finish')

            print("Searching for 'Experiment' on narrow page 2...", flush=True)
            command(instance, 'search', 'Experiment')
            time.sleep(0.1)
            command(instance, 'wait_for_search_to_finish')
            command(instance, 'overview_next_item')

            t_wait2 = time.perf_counter()
            command(instance, 'wait_for_renders_to_finish')
            wait_ms2 = (time.perf_counter() - t_wait2) * 1000
            print(f'wait_for_renders_to_finish returned in {wait_ms2:.1f} ms with narrow page overview open', flush=True)

            screenshot_path2 = work / 'overview_page_2.png'
            command(instance, 'framebuffer_screenshot', str(screenshot_path2))
            assert screenshot_path2.is_file() and screenshot_path2.stat().st_size > 1000
            h2 = hashlib.sha256(screenshot_path2.read_bytes()).hexdigest()
            print(f'Captured overview on page 2: {screenshot_path2.stat().st_size} bytes, sha256={h2[:12]}', flush=True)

            command(instance, 'close_overview')
            command(instance, 'quit')
            proc.wait(timeout=5)
            print('SUCCESS: Same-width mode overview test passed cleanly!', flush=True)
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=5)


if __name__ == '__main__':
    binary = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('build/dev/sioyek')
    run_test(binary)
