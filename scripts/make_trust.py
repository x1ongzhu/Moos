#!/usr/bin/env python3
"""Read the official DER root and emit a BearSSL RSA trust anchor (no deps)."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
der = (root / 'tls/digicert-global-root-g2.der').read_bytes()
def tlv(data, at=0):
    tag = data[at]; n = data[at+1]; p = at+2
    if n & 128:
        count = n & 127; n = int.from_bytes(data[p:p+count], 'big'); p += count
    return tag, data[p:p+n], data[at:p+n], p+n
_, cert, _, _ = tlv(der)
_, tbs, _, _ = tlv(cert)
at = 0
fields = []
while at < len(tbs):
    tag, value, raw, at = tlv(tbs, at); fields.append((tag, value, raw))
if fields[0][0] == 0xa0:
    fields.pop(0)
subject = fields[4][2]
spki = fields[5][1]
_, _, _, at = tlv(spki)
_, bitstring, _, _ = tlv(spki, at)
_, rsa, _, _ = tlv(bitstring[1:])
_, modulus, _, at = tlv(rsa)
_, exponent, _, _ = tlv(rsa, at)
modulus = modulus.lstrip(b'\0')
def arr(name, data):
    return 'static const unsigned char '+name+'[] = {'+','.join('0x%02x' % b for b in data)+'};\n'
s = '/* Generated from DigiCert official Global Root G2 DER certificate. */\n'
s += arr('root_dn', subject)+arr('root_n', modulus)+arr('root_e', exponent)
s += 'static const br_x509_trust_anchor roots[] = { { {(unsigned char *)root_dn, sizeof(root_dn)}, BR_X509_TA_CA, {BR_KEYTYPE_RSA, {.rsa = {(unsigned char *)root_n, sizeof(root_n), (unsigned char *)root_e, sizeof(root_e)}}} } };\n'
(root/'tls/trust.h').write_text(s)
