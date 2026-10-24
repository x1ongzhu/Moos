#!/usr/bin/env python3
"""Optional loopback HTTP -> OpenAI-compatible HTTPS relay; no dependencies.

MOSS_UPSTREAM=https://provider.example/v1 MOSS_UPSTREAM_KEY=... python3 tools/relay.py
Palm Base URL: http://127.0.0.1:8765/v1 (for Cloudpilot native networking).
"""
import argparse
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, build_opener, HTTPRedirectHandler


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class Relay(BaseHTTPRequestHandler):
    server_version = "MossRelay/0.1"

    def error(self, status, message):
        self.respond(status, json.dumps({"error": {"message": message}}).encode())

    def respond(self, status, body):
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path != "/v1/chat/completions":
            return self.error(404, "Use /v1/chat/completions")
        if self.headers.get("Transfer-Encoding"):
            return self.error(400, "Chunked requests are unsupported")
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            return self.error(400, "Invalid request length")
        if not 0 < length <= 8192:
            return self.error(413, "Request must be 1..8192 bytes")
        if self.server.relay_key:
            if self.headers.get("Authorization") != "Bearer " + self.server.relay_key:
                return self.error(401, "Invalid relay key")
        try:
            body = self.rfile.read(length)
            payload = json.loads(body)
            if not isinstance(payload, dict):
                raise ValueError()
            if not isinstance(payload.get("model"), str) or not payload["model"]:
                raise ValueError()
            messages = payload.get("messages")
            if not isinstance(messages, list) or not messages or payload.get("stream") is not False:
                raise ValueError()
            if any(not isinstance(m, dict) or m.get("role") not in ("user", "assistant", "system")
                   or not isinstance(m.get("content"), str) for m in messages):
                raise ValueError()
        except (ValueError, UnicodeDecodeError):
            return self.error(400, "Expected non-streaming text Chat Completions request")
        headers = {"Content-Type": "application/json", "Accept": "application/json",
                   "Accept-Encoding": "identity"}
        key = self.server.upstream_key
        authorization = "Bearer " + key if key else self.headers.get("Authorization")
        if authorization:
            headers["Authorization"] = authorization
        req = Request(self.server.upstream + "/chat/completions", data=body, headers=headers)
        try:
            response = build_opener(NoRedirect).open(req, timeout=80)
        except HTTPError as exc:
            response = exc
        except (URLError, TimeoutError, OSError):
            return self.error(502, "Upstream connection failed")
        try:
            with response:
                raw = response.read(16001)
                status = response.code
        except (TimeoutError, OSError):
            return self.error(502, "Upstream response interrupted")
        if len(raw) > 16000:
            return self.error(502, "Upstream answer exceeds Palm response capacity; request a shorter answer")
        if 300 <= status < 400:
            return self.error(502, "Upstream redirects are unsupported; configure the final Base URL")
        return self.respond(status, raw)

    def log_message(self, fmt, *args):
        # Never log headers, API keys or message bodies.
        print("Moss relay:", fmt % args, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", default=os.environ.get("MOSS_UPSTREAM"))
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    url = urlsplit(args.upstream or "")
    if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password or url.query or url.fragment:
        parser.error("Set MOSS_UPSTREAM or --upstream to a valid compatible API Base URL")
    if os.environ.get("MOSS_RELAY_KEY") and not os.environ.get("MOSS_UPSTREAM_KEY"):
        parser.error("MOSS_RELAY_KEY requires MOSS_UPSTREAM_KEY to keep relay and provider credentials separate")
    server = ThreadingHTTPServer((args.bind, args.port), Relay)
    server.upstream = args.upstream.rstrip("/")
    server.upstream_key = os.environ.get("MOSS_UPSTREAM_KEY", "")
    server.relay_key = os.environ.get("MOSS_RELAY_KEY", "")
    print(f"Moss relay listening on http://{args.bind}:{args.port}/v1", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
