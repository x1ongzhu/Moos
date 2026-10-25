#!/usr/bin/env python3
"""Exercise relay forwarding, fixed routing, auth, and response limits locally."""
import importlib.util
import json
import http.client
import ssl
import subprocess
import tempfile
import time
from pathlib import Path
from http.server import BaseHTTPRequestHandler, HTTPServer, ThreadingHTTPServer
import threading
import unittest
from urllib.request import Request, urlopen
from urllib.error import HTTPError

spec = importlib.util.spec_from_file_location('relay', Path(__file__).resolve().parents[1] / 'tools/relay.py')
relay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(relay)

class Upstream(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def handle(self):
        try:
            super().handle()
        except ConnectionError:
            pass
    def do_POST(self):
        self.server.last_path = self.path
        self.server.last_auth = self.headers.get('Authorization')
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.server.requests.append((self.client_address[1], payload))
        if payload['model'] == 'disconnect':
            self.close_connection = True
            return
        if payload.get('stream') is True and payload['model'] != 'json-fallback':
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            pieces = [b': keepalive\n\n',
                      b'data: {"choices":[{"delta":{"content":"First "}}]}\n\n',
                      b'data: {"choices":[{"delta":{"content":"second"}}]}\n\n',
                      b'data: [DONE]\n\n']
            for index, piece in enumerate(pieces):
                if index == 3 and payload['model'] == 'stream-no-done':
                    break
                if index == 2 and payload['model'] == 'stream-error':
                    piece = b'data: {"error":{"message":"provider failed"}}\n\n'
                self.wfile.write(f'{len(piece):x}\r\n'.encode() + piece + b'\r\n')
                self.wfile.flush()
                if index == 1:
                    if payload['model'] == 'stream-drop':
                        self.close_connection = True
                        return
                    if payload['model'] == 'stream-gated':
                        self.server.first_event.set()
                        self.server.gate_timed_out = not self.server.release_stream.wait(5)
            self.wfile.write(b'0\r\n\r\n')
            self.wfile.flush()
            return
        content = 'x' * 17000 if payload['model'] == 'large' else 'OK'
        body = json.dumps({'choices': [{'message': {'content': content}}]}).encode()
        self.send_response(302 if payload['model'] == 'redirect' else
                           401 if payload['model'] == 'unauthorized' else 200)
        if payload['model'] == 'compressed':
            self.send_header('Content-Encoding', 'gzip')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *args):
        pass

class RelayTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.upstream = ThreadingHTTPServer(('127.0.0.1', 0), Upstream)
        cls.upstream.requests = []
        cls.upstream.first_event = threading.Event()
        cls.upstream.release_stream = threading.Event()
        cls.upstream.gate_timed_out = False
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), relay.Relay)
        cls.server.upstream = f'http://127.0.0.1:{cls.upstream.server_port}/custom/v1'
        cls.server.upstream_key = 'upstream-test-key'
        cls.server.relay_key = 'relay-test-key'
        for server in (cls.upstream, cls.server):
            threading.Thread(target=server.serve_forever, daemon=True).start()
    @classmethod
    def tearDownClass(cls):
        for server in (cls.server, cls.upstream):
            server.shutdown()
            server.server_close()
    def post(self, path='/v1/chat/completions', key='relay-test-key', model='test', extra=None):
        body = {'model': model, 'messages': [{'role': 'user', 'content': 'hello'}], 'stream': False}
        if extra:
            body.update(extra)
        request = Request(f'http://127.0.0.1:{self.server.server_port}{path}',
            data=json.dumps(body).encode(), headers={'Authorization': 'Bearer ' + key, 'Content-Type': 'application/json'})
        try:
            response = urlopen(request, timeout=3)
        except HTTPError as error:
            response = error
        with response:
            return response.code, json.loads(response.read())
    def test_forward(self):
        status, body = self.post()
        self.assertEqual(status, 200)
        self.assertEqual(body['choices'][0]['message']['content'], 'OK')
        self.assertEqual(self.upstream.last_path, '/custom/v1/chat/completions')
        self.assertEqual(self.upstream.last_auth, 'Bearer upstream-test-key')
    def test_auth(self):
        self.assertEqual(self.post(key='wrong')[0], 401)
    def test_fixed_route(self):
        self.assertEqual(self.post(path='/elsewhere')[0], 404)
    def test_invalid_stream(self):
        self.assertEqual(self.post(extra={'stream': 'true'})[0], 400)
    def test_capacity(self):
        self.assertEqual(self.post(model='large')[0], 502)

    def test_persistent_connections(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        body = json.dumps({'model': 'test', 'messages': [{'role': 'user', 'content': 'hello'}], 'stream': False})
        try:
            ports = []
            for _ in range(2):
                conn.request('POST', '/v1/chat/completions', body,
                             {'Authorization': 'Bearer relay-test-key', 'Content-Type': 'application/json'})
                response = conn.getresponse()
                self.assertEqual(response.status, 200)
                self.assertEqual(response.getheader('Connection'), 'keep-alive')
                self.assertEqual(json.loads(response.read())['choices'][0]['message']['content'], 'OK')
                ports.append(self.upstream.requests[-1][0])
            self.assertEqual(ports[0], ports[1], 'upstream TCP connection was not reused')
        finally:
            conn.close()

    def test_no_post_replay(self):
        before = len(self.upstream.requests)
        self.assertEqual(self.post(model='disconnect')[0], 502)
        self.assertEqual(len(self.upstream.requests), before + 1)

    def test_redirect(self):
        self.assertEqual(self.post(model='redirect')[0], 502)

    def test_upstream_status(self):
        self.assertEqual(self.post(model='unauthorized')[0], 401)

    def test_compression(self):
        self.assertEqual(self.post(model='compressed')[0], 502)

    def test_duplicate_length(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        try:
            conn.putrequest('POST', '/v1/chat/completions')
            conn.putheader('Content-Length', '2')
            conn.putheader('Content-Length', '2')
            conn.endheaders(b'{}')
            response = conn.getresponse()
            self.assertEqual(response.status, 400)
            self.assertEqual(response.getheader('Connection'), 'close')
            response.read()
        finally:
            conn.close()

    def test_stream_before_completion(self):
        self.upstream.first_event.clear()
        self.upstream.release_stream.clear()
        self.upstream.gate_timed_out = False
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        payload = json.dumps({'model': 'stream-gated', 'stream': True,
                              'messages': [{'role': 'user', 'content': 'test'}]})
        try:
            conn.request('POST', '/v1/chat/completions', payload,
                         {'Authorization': 'Bearer relay-test-key'})
            response = conn.getresponse()
            self.assertEqual(response.getheader('Content-Type'), 'text/plain; charset=utf-8')
            first = b''
            while b'First ' not in first:
                first += response.read1(4096)
            self.assertFalse(self.upstream.gate_timed_out)
            self.assertNotIn(b'[DONE]', first)
            self.upstream.release_stream.set()
            rest = response.read()
            self.assertIn(b'second', rest)
            self.assertNotIn(b'[DONE]', rest)
            self.assertNotIn(b'data:', first + rest)
            self.assertEqual(first + rest, b'First second')
        finally:
            self.upstream.release_stream.set()
            conn.close()

    def test_stream_reuse(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        payload = json.dumps({'model': 'stream', 'stream': True,
                              'messages': [{'role': 'user', 'content': 'test'}]})
        try:
            ports = []
            for _ in range(2):
                conn.request('POST', '/v1/chat/completions', payload,
                             {'Authorization': 'Bearer relay-test-key'})
                response = conn.getresponse()
                self.assertEqual(response.status, 200)
                self.assertEqual(response.getheader('Connection'), 'keep-alive')
                self.assertEqual(response.read(), b'First second')
                ports.append(self.upstream.requests[-1][0])
            self.assertEqual(ports[0], ports[1])
        finally:
            conn.close()

    def test_stream_interrupted(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        payload = json.dumps({'model': 'stream-drop', 'stream': True,
                              'messages': [{'role': 'user', 'content': 'test'}]})
        try:
            before = len(self.upstream.requests)
            conn.request('POST', '/v1/chat/completions', payload,
                         {'Authorization': 'Bearer relay-test-key'})
            response = conn.getresponse()
            with self.assertRaises(http.client.IncompleteRead) as error:
                response.read()
            self.assertEqual(error.exception.partial, b'First ')
            self.assertEqual(len(self.upstream.requests), before + 1)
        finally:
            conn.close()

    def test_sse_decoder_fragmentation(self):
        events = ('\ufeff: heartbeat\r\n\r\ndata: {"choices":\n'
                  'data: [{"index":0,"delta":{"content":"Hi \u4f60\u597d \\ud83d\\ude00"}}]}\n\n'
                  'data: {"choices":[],"usage":{}}\r\rdata: [DONE]\r\n\r\n').encode()
        for split in range(len(events) + 1):
            decoder = relay.SSETextDecoder()
            text = decoder.feed(events[:split]) + decoder.feed(events[split:])
            decoder.finish()
            self.assertEqual(text, 'Hi \u4f60\u597d \U0001f600'.encode())
        self.assertLess(len(text), len(events) // 4)

    def test_stream_json_fallback(self):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        try:
            conn.request('POST', '/v1/chat/completions', json.dumps({
                'model': 'json-fallback', 'stream': True,
                'messages': [{'role': 'user', 'content': 'test'}]}),
                {'Authorization': 'Bearer relay-test-key', 'Accept': 'text/plain'})
            response = conn.getresponse()
            self.assertEqual(response.getheader('Content-Type'), 'text/plain; charset=utf-8')
            self.assertEqual(response.read(), b'OK')
        finally:
            conn.close()

    def test_stream_invalid_completion(self):
        for model in ('stream-no-done', 'stream-error'):
            with self.subTest(model=model):
                conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
                try:
                    conn.request('POST', '/v1/chat/completions', json.dumps({
                        'model': model, 'stream': True,
                        'messages': [{'role': 'user', 'content': 'test'}]}),
                        {'Authorization': 'Bearer relay-test-key', 'Accept': 'text/plain'})
                    response = conn.getresponse()
                    with self.assertRaises(http.client.IncompleteRead) as error:
                        response.read()
                    self.assertIn(b'First ', error.exception.partial)
                    self.assertNotIn(b'data:', error.exception.partial)
                    self.assertNotIn(b'provider failed', error.exception.partial)
                finally:
                    conn.close()

    def test_missing_done_rejected(self):
        decoder = relay.SSETextDecoder()
        self.assertEqual(decoder.feed(b'data: {"choices":[{"delta":{"content":"partial"}}]}\n\n'), b'partial')
        with self.assertRaises(ValueError):
            decoder.finish()

    def test_plain_error(self):
        request = Request(f'http://127.0.0.1:{self.server.server_port}/v1/chat/completions',
                          data=b'{}', headers={'Accept': 'text/plain'})
        with self.assertRaises(HTTPError) as error:
            urlopen(request, timeout=3)
        response = error.exception
        try:
            self.assertEqual(response.code, 401)
            self.assertEqual(response.getheader('Content-Type'), 'text/plain; charset=utf-8')
            self.assertEqual(response.read(), b'Invalid relay key')
        finally:
            response.close()

    def test_https_certificate_rejection(self):
        original = self.server.upstream
        with tempfile.TemporaryDirectory(prefix='moss-relay-cert-') as directory:
            cert, key = Path(directory) / 'cert.pem', Path(directory) / 'key.pem'
            subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                            '-days', '1', '-subj', '/CN=localhost', '-keyout', str(key),
                            '-out', str(cert)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(cert, key)
            server = ThreadingHTTPServer(('127.0.0.1', 0), Upstream)
            server.requests = []
            server.socket = context.wrap_socket(server.socket, server_side=True)
            threading.Thread(target=server.serve_forever, daemon=True).start()
            try:
                self.server.upstream = f'https://127.0.0.1:{server.server_port}/v1'
                self.assertEqual(self.post()[0], 502)
                self.assertEqual(server.requests, [], 'HTTP was released before certificate validation')
            finally:
                self.server.upstream = original
                server.shutdown()
                server.server_close()

if __name__ == '__main__':
    unittest.main()
