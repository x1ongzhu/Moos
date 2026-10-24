#!/usr/bin/env python3
"""Compare both RSA paths against Python modular exponentiation, offline."""
from pathlib import Path
import os
import random
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
vendor = root / 'vendor/bearssl-0.6'
rng = random.Random(65537)
vectors = []
for bits in (1024, 2048, 4096):
    for exponent in (3, 17, 65537):
        for _ in range(3):
            modulus = rng.getrandbits(bits) | (1 << (bits - 1)) | 1
            value = rng.randrange(modulus)
            width = bits // 8
            vectors.append(' '.join((
                modulus.to_bytes(width, 'big').hex(),
                value.to_bytes(width, 'big').hex(),
                exponent.to_bytes(3, 'big').hex(),
                pow(value, exponent, modulus).to_bytes(width, 'big').hex())))
# Also exercise leading-zero exponents and values at the modulus boundary.
for exponent in (1, 3, 65537):
    modulus = (1 << 2048) - 159
    for value in (0, 1, modulus - 1):
        vectors.append(' '.join((
            modulus.to_bytes(256, 'big').hex(),
            value.to_bytes(256, 'big').hex(),
            exponent.to_bytes(4, 'big').hex(),
            pow(value, exponent, modulus).to_bytes(256, 'big').hex())))

sources = [root / 'tests/rsa_public_test.c', root / 'tls/rsa_verify.c']
sources += sorted((vendor / 'src/int').glob('i15*.c'))
sources += [vendor / 'src/int/i32_div32.c',
            vendor / 'src/rsa/rsa_i15_pub.c',
            vendor / 'src/rsa/rsa_pkcs1_sig_unpad.c',
            vendor / 'src/codec/ccopy.c']
with tempfile.TemporaryDirectory(prefix='moss-rsa-') as directory:
    binary = Path(directory) / 'rsa_test'
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += ['-O2', '-fsanitize=address,undefined',
                '-I' + str(vendor / 'inc'), '-I' + str(vendor / 'src'),
                '-I' + str(root / 'tls')]
    subprocess.run(command + [str(s) for s in sources] + ['-o', str(binary)], check=True)
    subprocess.run([str(binary)], input='\n'.join(vectors) + '\n', text=True, check=True)
