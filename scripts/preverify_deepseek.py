#!/usr/bin/env python3
"""Export only public, fully validated DeepSeek certificate metadata."""
from pathlib import Path
import hashlib
import os
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
header = root / 'tls/preverified_deepseek.h'
policy = hashlib.sha256((root / 'tls/trust.h').read_bytes()).hexdigest()
marker = '/* Trust policy SHA-256: ' + policy + ' */'
if sys.argv[1:] == ['--check']:
    content = header.read_text()
    if '#define MOSS_HAS_PREVERIFIED 1' in content and marker not in content:
        raise SystemExit('Trust policy changed: regenerate the preverified certificate cache.')
elif sys.argv[1:]:
    raise SystemExit('Usage: preverify_deepseek.py [--check]')
else:
    with tempfile.TemporaryDirectory(prefix='moss-preverify-') as directory:
        exported = Path(directory) / 'cache.h'
        environment = os.environ.copy()
        environment.update(MOSS_EXPORT_CACHE=str(exported), MOSS_TLS_HOST='api.deepseek.com',
                           MOSS_TLS_PORT='443', MOSS_TLS_TEST_SOURCE=str(root / 'tests/tls_handshake.c'))
        subprocess.run(['bash', str(root / 'tests/tls_handshake.sh')], env=environment, check=True)
        content = exported.read_text()
        if not re.search(r'1UL,\d+UL,\d+UL,\d+UL,\d+UL', content):
            raise SystemExit('Missing verified certificate validity interval')
        header.write_text(content + marker + '\n')
        print('Saved verified public certificate fingerprint and validity interval; no API key used.')
