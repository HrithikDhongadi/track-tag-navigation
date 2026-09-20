#!/usr/bin/env python3
"""Serve the local map editor at http://127.0.0.1:8000/."""
import argparse
from io import BytesIO
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import os
from urllib.parse import unquote, urlparse
import webbrowser
import qrcode


class EditorHandler(SimpleHTTPRequestHandler):
    """Static editor files plus generated, preview-only QR PNGs."""
    def do_GET(self):
        path = urlparse(self.path).path
        if not path.startswith('/api/qr/') or not path.endswith('.png'):
            return super().do_GET()
        identifier = unquote(path[len('/api/qr/'):-4])
        if not identifier or len(identifier) > 80 or any(ord(char) < 32 for char in identifier):
            self.send_error(400, 'Invalid QR identifier')
            return
        qr = qrcode.QRCode(version=1, error_correction=qrcode.constants.ERROR_CORRECT_M,
                           box_size=16, border=4)
        try:
            qr.add_data(identifier)
            qr.make(fit=False)
        except ValueError:
            self.send_error(400, 'Identifier does not fit version-1 QR')
            return
        output = BytesIO()
        qr.make_image(fill_color='black', back_color='white').convert('RGB').save(output, 'PNG')
        data = output.getvalue()
        self.send_response(200)
        self.send_header('Content-Type', 'image/png')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', type=int, default=8000)
parser.add_argument('--no-open', action='store_true', help='do not open the browser')
args = parser.parse_args()
PROJECT_ROOT = Path(__file__).resolve().parents[1]
os.chdir(PROJECT_ROOT)
url = f'http://127.0.0.1:{args.port}/tools/map_editor/'
print(f'Map editor: {url}')
if not args.no_open:
    webbrowser.open(url)
ThreadingHTTPServer(('127.0.0.1', args.port), EditorHandler).serve_forever()
