#!/usr/bin/env python3
"""Create one unique initial seed PDB per Palm device; never overwrite a file."""
import argparse, os, struct, time
from pathlib import Path

def provision(path):
    now = int(time.time()) + 2082844800
    header = struct.pack('>32sHHIIIIII4s4sIIH', b'MossTLS Entropy', 0, 1,
                         now, now, 0, 0, 0, 0, b'MTSE', b'MTLS', 2, 0, 1)
    data = header + struct.pack('>IB3s', 88, 0, b'\0\0\1') + b'\0\0' + os.urandom(32)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as target:
        target.write(data)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    provision(Path(args.output))
    print('Created unique initial seed; install once on one Palm device.')
