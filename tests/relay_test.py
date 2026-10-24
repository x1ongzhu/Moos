#!/usr/bin/env python3
"""Exercise relay forwarding, fixed routing, auth, and response limits locally."""
import importlib.util
import json
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
    def do_POST(self):
        self.server.last_path = self.path
        self.server.last_auth = self.headers.get('Authorization')
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        content = 'x' * 17000 if payload['model'] == 'large' else 'OK'
        body = json.dumps({'choices': [{'message': {'content': content}}]}).encode()
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *args):
        pass

class RelayTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.upstream = HTTPServer(('127.0.0.1', 0), Upstream)
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
    def test_non_streaming(self):
        self.assertEqual(self.post(extra={'stream': True})[0], 400)
    def test_capacity(self):
        self.assertEqual(self.post(model='large')[0], 502)

if __name__ == '__main__':
    unittest.main()
