#!/usr/bin/env python3
"""Package remote-compiled resources. This performs no compilation locally."""
from pathlib import Path
import re,struct,time,os
root=Path(__file__).resolve().parents[1]
def prc(name,kind,creator,resources):
    now=int(time.time())+2082844800
    header=struct.pack('>32sHHIIIIII4s4sIIH',name.encode(),1,1,now,now,0,0,0,0,kind,creator,0,0,len(resources))
    offset=78+10*len(resources)+2
    table=bytearray(); body=bytearray()
    for typ,rid,data in resources:
        table+=struct.pack('>4sHI',typ,rid,offset);body+=data;offset+=len(data)
    return header+table+b'\0\0'+body
symbols={};undefined=[]
for line in (root/'build/tls/core.symbols').read_text().splitlines():
    parts=line.split()
    if len(parts)==2 and parts[0]=='U': undefined.append(parts[1])
    if len(parts)==3: symbols[parts[2]]=int(parts[0],16)
if undefined: raise SystemExit('Unresolved TLS symbols: '+', '.join(undefined))
image=(root/'build/tls/core.bin').read_bytes();offsets=[]
for line in (root/'build/tls/core.relocs').read_text().splitlines():
    m=re.match(r'^([0-9a-fA-F]{8})\s+(\S+)\s+',line)
    if not m: continue
    offset=int(m[1],16);kind=m[2]
    if kind=='32':
        assert offset+4<=len(image) and struct.unpack_from('>I',image,offset)[0]<len(image)
        offsets.append(offset)
    elif kind!='DISP16': raise SystemExit('Unsupported relocation '+kind)
assert len(image)<65536
meta=struct.pack('>II',symbols['MossTLSCore'],len(offsets))+b''.join(struct.pack('>I',n) for n in offsets)
shim=(root/'build/tls/shim.bin').read_bytes()
tls_name='MossTLS-insecure.prc' if os.environ.get('MOSS_TLS_INSECURE')=='1' else 'MossTLS.prc'
(root/'dist'/tls_name).write_bytes(prc('Moss TLS Library',b'libr',b'MTLS',[(b'libr',0,shim),(b'mtCD',1000,image),(b'mtRL',1000,meta)]))
# Each Palm installation needs its own entropy seed. Existing provision is retained.
seed=root/'dist/MossTLSEntropy.pdb'
if not seed.exists():
    from provision_entropy import provision
    provision(seed)
print('TLS library:',len(shim),'bytes shim,',len(image),'bytes crypto,',len(offsets),'relocations')
