#!/usr/bin/env python3
"""Make an optional one-shot Palm API configuration PDB. Key is read from stdin."""
import argparse,getpass,struct,time,os
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--url',required=True);p.add_argument('--model',required=True)
p.add_argument('--output',required=True);p.add_argument('--key-file')
a=p.parse_args()
key=Path(a.key_file).read_text().strip() if a.key_file else getpass.getpass('API Key: ')
def fixed(s,n):
    b=s.encode('ascii'); assert len(b)<n; return b+b'\0'*(n-len(b))
body=struct.pack('>H',1)+fixed(a.url,193)+fixed(a.model,64)+fixed(key,192)+b'\0'
assert len(body)==452
now=int(time.time())+2082844800
header=struct.pack('>32sHHIIIIII4s4sIIH',b'Moss Test Config',0,1,now,now,0,0,0,0,b'MCFG',b'Moss',2,0,1)
fd=os.open(a.output,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
with os.fdopen(fd,'wb') as f:f.write(header+struct.pack('>IB3s',88,0,b'\0\0\1')+b'\0\0'+body)
print('One-shot API configuration created; key omitted from output.')
