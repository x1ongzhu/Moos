#!/usr/bin/env python3
"""Local deterministic streaming fixture. Does not call any AI provider."""
import argparse
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def chunk(self, data):
        self.wfile.write(f'{len(data):x}\r\n'.encode() + data + b'\r\n')
        self.wfile.flush()

    def do_POST(self):
        if self.path != '/v1/chat/completions':
            self.send_error(404)
            return
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert payload['model'] == 'mock-model'
        assert all(isinstance(m['content'], str) for m in payload['messages'])
        question = payload['messages'][-1]['content']
        text = 'Mock reply: ' + question + '\nContext messages: ' + str(len(payload['messages']))
        if question == 'error':
            body = b'{"error":{"message":"Test authentication error"}}'
            self.send_response(401)
        elif payload['stream']:
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            try:
                for i in range(0, len(text), 4):
                    event = {'choices': [{'index': 0, 'delta': {'content': text[i:i + 4]}}]}
                    self.chunk(b'data: ' + json.dumps(event).encode() + b'\n\n')
                    time.sleep(self.server.delay)
                if question != 'interrupt':
                    self.chunk(b'data: [DONE]\n\n')
                self.wfile.write(b'0\r\n\r\n')
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                self.close_connection = True
            return
        else:
            body = json.dumps({'choices': [{'message': {'content': text}}]}).encode()
            self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8766)
    parser.add_argument('--delay', type=float, default=0.05)
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    server.delay = args.delay
    print(f'Mock API on http://127.0.0.1:{args.port}/v1; model=mock-model; no real AI calls', flush=True)
    server.serve_forever()
