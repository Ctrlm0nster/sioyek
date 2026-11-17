#!/usr/bin/env python3
"""Create bounded, deterministic synthetic PDFs using only Python's standard library.

These fixtures exercise text, image-only pages and native PDF annotations. They
are not a substitute for testing selected real documents. No personal files used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zlib


def write_fixture(path, data):
    try:
        with path.open('xb') as stream:
            stream.write(data)
    except FileExistsError:
        if path.read_bytes() != data:
            raise FileExistsError(f'Refusing to overwrite different content: {path}') from None


class PDF:
    def __init__(self):
        self.objects = [b'', b'']  # catalog and pages tree
        self.pages = []
        self.font = self.add(b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>')

    def add(self, data):
        self.objects.append(data)
        return len(self.objects)

    def stream(self, data, attributes=b''):
        packed = zlib.compress(data, 6)
        return self.add(b'<< /Filter /FlateDecode /Length ' + str(len(packed)).encode() +
                        b' ' + attributes + b' >>\nstream\n' + packed + b'\nendstream')

    def page(self, content, resources=b'', annotations=()):
        stream = self.stream(content)
        refs = b' '.join(f'{i} 0 R'.encode() for i in annotations)
        page = self.add(f'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents {stream} 0 R '.encode() +
                        f'/Resources << /Font << /F1 {self.font} 0 R >> '.encode() + resources +
                        b' >> /Annots [' + refs + b'] >>')
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
        if len(output) > 32 * 1024 * 1024:
            raise RuntimeError('Fixture exceeds the 32 MiB per-file limit')
        write_fixture(path, output)
        return {'pages': len(self.pages), 'bytes': len(output),
                'sha256': hashlib.sha256(output).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, count in [('large-text', 240), ('annotated', 32), ('image-only', 32)]:
        pdf = PDF()
        for page in range(count):
            if name == 'image-only':
                width, height = 768, 1024
                # Synthetic scan-like raster: each page has distinct pixels.
                pixels = b''.join(bytes((x // 3 + y * 3 + page * 17 + (x % 3) * 53) % 256
                                        for x in range(width * 3)) for y in range(height))
                image = pdf.stream(pixels, f'/Type /XObject /Subtype /Image /Width {width} /Height {height} /ColorSpace /DeviceRGB /BitsPerComponent 8'.encode())
                pdf.page(b'q 612 0 0 792 0 0 cm /Scan Do Q', f'/XObject << /Scan {image} 0 R >>'.encode())
                continue
            text = [f'BT /F1 11 Tf 40 750 Td 14 TL (Sioyek benchmark page {page + 1}) Tj'.encode()]
            text += [b'T* (The reader searches and scrolls this deterministic Sioyek fixture.) Tj'] * 45
            annotations = []
            if name == 'annotated':
                for line in range(36):
                    y = 736 - line * 14
                    annotations.append(pdf.add(f'<< /Type /Annot /Subtype /Highlight /Rect [40 {y-3} 390 {y+10}] /QuadPoints [40 {y+10} 390 {y+10} 40 {y-3} 390 {y-3}] /C [1 1 0] /CA 0.25 /F 4 /Contents (Benchmark annotation) >>'.encode()))
            pdf.page(b'\n'.join(text) + b'\nET', annotations=annotations)
        manifest[name] = pdf.write(args.output / f'{name}.pdf')
    write_fixture(args.output / 'manifest.json', (json.dumps(manifest, indent=2) + '\n').encode())
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
