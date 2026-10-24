#!/usr/bin/env python3
"""A completed TLS handshake with an untrusted peer must release no HTTP data."""
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

with tempfile.TemporaryDirectory(prefix='moss-untrusted-') as directory:
    certificate = Path(directory) / 'certificate.pem'
    key = Path(directory) / 'key.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-keyout', str(key), '-out', str(certificate), '-days', '1',
                    '-subj', '/CN=api.deepseek.com'], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.set_ciphers('AES128-GCM-SHA256')
    context.load_cert_chain(certificate, key)
    received, failures = [], []
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(10)

        def serve():
            try:
                connection, _ = listener.accept()
                with connection:
                    connection.settimeout(10)
                    with context.wrap_socket(connection, server_side=True) as peer:
                        received.append(peer.recv(4096))
            except Exception as error:
                failures.append(repr(error))

        worker = threading.Thread(target=serve, daemon=True)
        worker.start()
        result = subprocess.run([sys.argv[1], '127.0.0.1',
                                 str(listener.getsockname()[1]), 'api.deepseek.com'],
                                capture_output=True, text=True, timeout=20)
        worker.join(timeout=12)
    if result.returncode != 6 or 'X509=62' not in result.stdout:
        raise SystemExit('Untrusted certificate was not rejected:\n' + result.stdout + result.stderr)
    if worker.is_alive() or failures or received != [b'']:
        raise SystemExit('Untrusted peer received data or test failed: ' + repr((received, failures)))
    print('Untrusted peer rejected after handshake; no application bytes released')
