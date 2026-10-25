#!/usr/bin/env python3
"""Palm HTTP -> OpenAI-compatible API relay, using only the Python standard library.

MOSS_UPSTREAM=https://provider.example/v1 MOSS_UPSTREAM_KEY=... python3 tools/relay.py
Palm Relay URL: http://127.0.0.1:8765/v1 (Cloudpilot native networking).
"""
import argparse
import codecs
import hmac
import http.client
import json
import os
import ssl
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


class SSETextDecoder:
    """Bounded upstream SSE parser; downstream receives only UTF-8 answer text."""
    def __init__(self):
        self.decoder = codecs.getincrementaldecoder("utf-8")()
        self.line = []
        self.data = []
        self.event_size = 0
        self.after_cr = False
        self.first = True
        self.done = False

    def event(self):
        value = "\n".join(self.data)
        self.data = []
        self.event_size = 0
        if not value:
            return ""
        if self.done:
            raise ValueError("Data after upstream completion")
        if value == "[DONE]":
            self.done = True
            return ""
        event = json.loads(value)
        if not isinstance(event, dict) or "error" in event:
            raise ValueError("Upstream error event")
        choices = event.get("choices")
        if not isinstance(choices, list):
            raise ValueError("Missing upstream choices")
        texts = []
        for position, choice in enumerate(choices):
            if not isinstance(choice, dict):
                raise ValueError("Invalid upstream choice")
            if choice.get("index", position) != 0:
                continue
            delta = choice.get("delta")
            if not isinstance(delta, dict):
                raise ValueError("Missing upstream delta")
            content = delta.get("content")
            if content is None:
                continue
            if not isinstance(content, str) or "\0" in content:
                raise ValueError("Invalid upstream text")
            texts.append(content)
        return "".join(texts)

    def end_line(self):
        line = "".join(self.line)
        self.line = []
        if not line:
            return self.event()
        field, separator, value = line.partition(":")
        if field == "data":
            if value.startswith(" "):
                value = value[1:]
            self.event_size += len(value) + 1
            if self.event_size > 65536:
                raise ValueError("Upstream SSE event too large")
            self.data.append(value)
        return ""

    def feed(self, raw):
        texts = []
        for char in self.decoder.decode(raw):
            if self.first:
                self.first = False
                if char == "\ufeff":
                    continue
            if self.after_cr:
                self.after_cr = False
                if char == "\n":
                    continue
            if char in "\r\n":
                texts.append(self.end_line())
                self.after_cr = char == "\r"
            else:
                self.line.append(char)
                if len(self.line) > 65536:
                    raise ValueError("Upstream SSE line too large")
        return "".join(texts).encode("utf-8")

    def finish(self):
        self.decoder.decode(b"", final=True)
        if not self.done:
            raise ValueError("Upstream ended before [DONE]")


class Relay(BaseHTTPRequestHandler):
    server_version = "MossRelay/0.2"
    protocol_version = "HTTP/1.1"

    def setup(self):
        super().setup()
        self.connection.settimeout(90)
        self.upstream_connection = None
        self.upstream_used = 0

    def handle(self):
        try:
            super().handle()
        except (ConnectionError, TimeoutError):
            self.close_connection = True

    def close_upstream(self):
        if self.upstream_connection is not None:
            self.upstream_connection.close()
            self.upstream_connection = None

    def finish(self):
        try:
            self.close_upstream()
        finally:
            super().finish()

    def error(self, status, message):
        # Validation failures may leave an unread request body: never reuse it.
        self.close_connection = True
        if "text/plain" in self.headers.get("Accept", "").lower():
            self.respond(status, message.encode(), "text/plain; charset=utf-8")
        else:
            self.respond(status, json.dumps({"error": {"message": message}}).encode())

    def respond(self, status, body, content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close" if self.close_connection else "keep-alive")
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()

    def upstream(self):
        # Each Palm connection owns one upstream connection; no shared socket races.
        if time.monotonic() - self.upstream_used > 50:
            self.close_upstream()
        if self.upstream_connection is None:
            url = urlsplit(self.server.upstream)
            if url.scheme == "https":
                self.upstream_connection = http.client.HTTPSConnection(
                    url.hostname, url.port, timeout=80, context=ssl.create_default_context())
            else:
                self.upstream_connection = http.client.HTTPConnection(url.hostname, url.port, timeout=80)
        return self.upstream_connection

    def write_chunk(self, data):
        self.wfile.write(f"{len(data):x}\r\n".encode() + data + b"\r\n")
        self.wfile.flush()

    def stream_response(self, response):
        self.send_response(response.status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close" if self.close_connection else "keep-alive")
        self.end_headers()
        self.wfile.flush()
        total = 0
        decoder = SSETextDecoder()
        started = time.monotonic()
        try:
            while True:
                # read1 returns available bytes, rather than waiting for 1024 bytes.
                data = response.read1(1024)
                if not data:
                    break
                total += len(data)
                if total > 1048576 or time.monotonic() - started > 180:
                    raise ValueError("Streaming response exceeds relay limits")
                text = decoder.feed(data)
                if text:
                    self.write_chunk(text)
            decoder.finish()
            self.wfile.write(b"0\r\n\r\n")
            self.wfile.flush()
            self.upstream_used = time.monotonic()
            if response.will_close:
                self.close_upstream()
        except (BrokenPipeError, ConnectionResetError):
            self.close_connection = True
            self.close_upstream()
        except (http.client.HTTPException, OSError, ValueError):
            self.close_connection = True
            self.close_upstream()
            # Leave chunked framing unfinished on failure. Plain body bytes must
            # never contain SSE/error envelopes, nor look like a complete answer.

    def do_POST(self):
        if self.path != "/v1/chat/completions":
            return self.error(404, "Use /v1/chat/completions")
        if self.headers.get("Transfer-Encoding"):
            return self.error(400, "Chunked requests are unsupported")
        lengths = self.headers.get_all("Content-Length", [])
        if len(lengths) != 1 or not lengths[0].isascii() or not lengths[0].isdigit():
            return self.error(400, "Expected one Content-Length")
        length = int(lengths[0])
        if not 0 < length <= 8192:
            return self.error(413, "Request must be 1..8192 bytes")
        if self.server.relay_key:
            actual = self.headers.get("Authorization", "").encode()
            expected = ("Bearer " + self.server.relay_key).encode()
            if not hmac.compare_digest(actual, expected):
                return self.error(401, "Invalid relay key")
        try:
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError()
            payload = json.loads(body)
            if not isinstance(payload, dict):
                raise ValueError()
            if not isinstance(payload.get("model"), str) or not payload["model"]:
                raise ValueError()
            messages = payload.get("messages")
            if not isinstance(messages, list) or not messages or type(payload.get("stream")) is not bool:
                raise ValueError()
            if any(not isinstance(m, dict) or m.get("role") not in ("user", "assistant", "system")
                   or not isinstance(m.get("content"), str) for m in messages):
                raise ValueError()
        except (ValueError, UnicodeDecodeError):
            return self.error(400, "Expected text Chat Completions request with stream true or false")
        headers = {"Content-Type": "application/json",
                   "Accept": "text/event-stream" if payload["stream"] else "application/json",
                   "Accept-Encoding": "identity"}
        key = self.server.upstream_key
        authorization = "Bearer " + key if key else self.headers.get("Authorization")
        if authorization:
            headers["Authorization"] = authorization
        path = urlsplit(self.server.upstream).path.rstrip("/") + "/chat/completions"
        try:
            upstream = self.upstream()
            upstream.request("POST", path, body=body, headers=headers)
            response = upstream.getresponse()
            with response:
                content_type = response.getheader("Content-Type", "").split(";", 1)[0].strip().lower()
                encoding = response.getheader("Content-Encoding", "identity").lower()
                if (payload["stream"] and 200 <= response.status < 300
                        and content_type == "text/event-stream" and encoding == "identity"):
                    return self.stream_response(response)
                raw = response.read(16001)
                status = response.status
                encoding = response.getheader("Content-Encoding", "identity")
                if response.will_close or len(raw) > 16000:
                    self.close_upstream()
            self.upstream_used = time.monotonic()
        except (http.client.HTTPException, OSError, ValueError):
            # No automatic POST replay: the provider may already have accepted it.
            self.close_upstream()
            return self.error(502, "Upstream connection failed; retry manually")
        if len(raw) > 16000:
            return self.error(502, "Upstream answer exceeds Palm response capacity; request a shorter answer")
        if encoding.lower() != "identity":
            return self.error(502, "Upstream compressed response is unsupported")
        if 300 <= status < 400:
            return self.error(502, "Upstream redirects are unsupported; configure the final Base URL")
        if payload["stream"] or "text/plain" in self.headers.get("Accept", "").lower():
            try:
                parsed = json.loads(raw)
                if not 200 <= status < 300:
                    message = parsed.get("error", {}).get("message", "Upstream request failed")
                    return self.respond(status, str(message).encode(), "text/plain; charset=utf-8")
                text = parsed["choices"][0]["message"]["content"]
                if not isinstance(text, str) or "\0" in text:
                    raise ValueError("Invalid upstream text")
                return self.respond(status, text.encode(), "text/plain; charset=utf-8")
            except (ValueError, KeyError, IndexError, TypeError, UnicodeError, AttributeError):
                return self.error(502, "Upstream returned no supported text answer")
        return self.respond(status, raw)

    def log_message(self, fmt, *args):
        # Do not log headers, credentials, paths supplied by clients or message bodies.
        if fmt == '"%s" %s %s':
            print("Moss relay: response", args[1], flush=True)


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
