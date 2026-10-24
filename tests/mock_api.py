#!/usr/bin/env python3
"""Local deterministic protocol fixture. Does not call any AI provider."""
from http.server import BaseHTTPRequestHandler, HTTPServer
import json

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/v1/chat/completions':
            self.send_error(404)
            return
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert payload['stream'] is False
        assert payload['model'] == 'mock-model'
        assert all(isinstance(m['content'], str) for m in payload['messages'])
        print(json.dumps(payload), flush=True)
        question = payload['messages'][-1]['content']
        if question == 'error':
            raw = b'{"error":{"message":"Test authentication error"}}'
            self.send_response(401)
        else:
            raw = json.dumps({'choices': [{'message': {'role': 'assistant', 'content':
                'Mock reply: ' + question + '\nContext messages: ' + str(len(payload['messages']))},
                'finish_reason': 'stop'}]}).encode()
            self.send_response(200)
        # Exercise chunk framing, including small chunks splitting JSON tokens.
        self.send_header('Transfer-Encoding', 'chunked')
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        for i in range(0, len(raw), 17):
            chunk = raw[i:i + 17]
            self.wfile.write(f'{len(chunk):x}\r\n'.encode() + chunk + b'\r\n')
        self.wfile.write(b'0\r\n\r\n')

if __name__ == '__main__':
    print('Mock API on http://127.0.0.1:8765/v1; model=mock-model', flush=True)
    HTTPServer(('127.0.0.1', 8765), Handler).serve_forever()
