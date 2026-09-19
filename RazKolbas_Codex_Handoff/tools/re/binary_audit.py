"""Read-only PE64 / MSF7 symbol inventory. Python standard library only.
Addresses are image-relative RVAs unless marked VA. No DLLs are loaded/executed.
"""
import struct as S, json, re, uuid, hashlib, csv, os
from pathlib import Path
ROOT=Path(os.environ.get('UPSCALER_WORK', '/mnt/data/upscaler_work')); OUT=Path(os.environ.get('UPSCALER_AUDIT_OUT', '/mnt/data/upscaler_raw_audit'))
def u16(b,o=0): return S.unpack_from('<H',b,o)[0]
def u32(b,o=0): return S.unpack_from('<I',b,o)[0]
def u64(b,o=0): return S.unpack_from('<Q',b,o)[0]
def cstr(b,o): return b[o:b.find(b'\0',o)].decode('utf-8','replace')
class PE:
 def __init__(self,p):
  self.path=Path(p);self.b=self.path.read_bytes();b=self.b
  self.pe=u32(b,0x3c);assert b[self.pe:self.pe+4]==b'PE\0\0'
  self.opt=self.pe+24;assert u16(b,self.opt)==0x20b
  self.base=u64(b,self.opt+24);self.sections=[]
  start=self.opt+u16(b,self.pe+20)
  for i in range(u16(b,self.pe+6)):
   o=start+i*40
   self.sections.append({'name':b[o:o+8].rstrip(b'\0').decode(),'vsize':u32(b,o+8),'rva':u32(b,o+12),'size':u32(b,o+16),'offset':u32(b,o+20),'flags':u32(b,o+36)})
  self.dirs=[(u32(b,self.opt+112+i*8),u32(b,self.opt+116+i*8)) for i in range(min(u32(b,self.opt+108),16))]
 def off(self,r):
  for s in self.sections:
   if s['rva']<=r<s['rva']+max(s['size'],s['vsize']):return r-s['rva']+s['offset']
  if r<u32(self.b,self.opt+60):return r
  raise ValueError(hex(r))
 def rva(self,o):
  for s in self.sections:
   if s['offset']<=o<s['offset']+s['size']:return o-s['offset']+s['rva']
  return o
 def string(self,r):return cstr(self.b,self.off(r))
 def exports(self):
  r,size=self.dirs[0]
  if not r:return []
  o=self.off(r);b=self.b;n=u32(b,o+24);fs=self.off(u32(b,o+28));ns=self.off(u32(b,o+32));os=self.off(u32(b,o+36));rows=[]
  for i in range(n):
   ordinal=u16(b,os+2*i);addr=u32(b,fs+4*ordinal)
   rows.append({'name':self.string(u32(b,ns+4*i)),'rva':hex(addr),'ordinal':ordinal+u32(b,o+16),'forwarder':self.string(addr) if r<=addr<r+size else None})
  return rows
 def imports(self):
  r,size=self.dirs[1]
  if not r:return []
  o=self.off(r);b=self.b;rows=[]
  for i in range(1024):
   d=o+i*20;oft,stamp,chain,name,iat=S.unpack_from('<IIIII',b,d)
   if not (oft or name or iat):break
   ns=[];t=self.off(oft or iat)
   for j in range(10000):
    v=u64(b,t+8*j)
    if not v:break
    ns.append({'name':'#'+str(v&0xffff) if v>>63 else self.string(v+2),'iat_rva':hex(iat+8*j)})
   rows.append({'dll':self.string(name),'symbols':ns})
  return rows
 def delay_imports(self):
  # IMAGE_DELAYLOAD_DESCRIPTOR; attributes bit 0 denotes RVA fields.
  r,size=self.dirs[13];rows=[]
  if not r:return rows
  for o in range(self.off(r),self.off(r)+size,32):
   a=S.unpack_from('<IIIIIIII',self.b,o)
   if not any(a):break
   def rv(v):return v if a[0]&1 else v-self.base
   name,iat,intable=rv(a[1]),rv(a[3]),rv(a[4]);ns=[]
   for j in range(10000):
    v=u64(self.b,self.off(intable)+j*8)
    if not v:break
    ns.append({'name':'#'+str(v&0xffff) if v>>63 else self.string(rv(v)+2),'iat_rva':hex(iat+8*j)})
   rows.append({'dll':self.string(name),'symbols':ns})
  return rows
 def debug(self):
  r,size=self.dirs[6]; rows=[]
  if not r:return rows
  o=self.off(r);b=self.b
  for i in range(size//28):
   d=o+i*28
   if u32(b,d+12)==2:
    raw=u32(b,d+24)
    if b[raw:raw+4]==b'RSDS':rows.append({'guid':str(uuid.UUID(bytes_le=b[raw+4:raw+20])),'age':u32(b,raw+20),'pdb_path':cstr(b,raw+24)})
  return rows
 def versions(self):
  rows=[]
  for m in re.finditer(b'\xbd\x04\xef\xfe',self.b):
   o=m.start()
   if o+52<=len(self.b) and u32(self.b,o+4)==0x10000:
    a,c,d,e=S.unpack_from('<IIII',self.b,o+8)
    rows.append({'file_version':f'{a>>16}.{a&65535}.{c>>16}.{c&65535}','product_version':f'{d>>16}.{d&65535}.{e>>16}.{e&65535}'})
  return rows
 def functions(self):
  r,size=self.dirs[3]
  if not r:return []
  b=self.b;o=self.off(r)
  return [(u32(b,i),u32(b,i+4)) for i in range(o,o+size,12)]
 def strings(self):
  rows=[]
  for s in self.sections:
   if s['name'] not in ['.rdata','.data','.rsrc']:continue
   b=self.b[s['offset']:s['offset']+s['size']]
   for m in re.finditer(rb'[\x20-\x7e]{5,}',b):
    rows.append({'rva':hex(s['rva']+m.start()),'encoding':'ascii','text':m.group().decode()})
   for m in re.finditer(rb'(?:[\x20-\x7e]\x00){5,}',b):
    rows.append({'rva':hex(s['rva']+m.start()),'encoding':'utf16le','text':m.group().decode('utf-16le')})
  return rows
class PDB:
 def __init__(self,p):
  self.path=Path(p);b=self.path.read_bytes();assert b.startswith(b'Microsoft C/C++ MSF 7.00');self.b=b
  self.bs=u32(b,32);numbytes=u32(b,44);mapblock=u32(b,52);nd=(numbytes+self.bs-1)//self.bs
  blocks=S.unpack_from('<'+'I'*nd,b,mapblock*self.bs)
  directory=b''.join(b[k*self.bs:(k+1)*self.bs] for k in blocks)[:numbytes]
  n=u32(directory);sizes=S.unpack_from('<'+'I'*n,directory,4);o=4+4*n;self.streams=[]
  for size in sizes:
   if size==0xffffffff:self.streams.append(b'');continue
   nb=(size+self.bs-1)//self.bs;ids=S.unpack_from('<'+'I'*nb,directory,o);o+=nb*4
   self.streams.append(b''.join(b[k*self.bs:(k+1)*self.bs] for k in ids)[:size])
 def identity(self):
  b=self.streams[1];return {'guid':str(uuid.UUID(bytes_le=b[12:28])),'age':u32(b,8),'stream_count':len(self.streams)}
 def modules(self):
  b=self.streams[3];end=64+u32(b,24);o=64;rows=[]
  while o<end:
   stream=u16(b,o+34);symbytes=u32(b,o+36);c13=u32(b,o+44)
   name=cstr(b,o+64);no=o+64+len(name.encode())+1;obj=cstr(b,no)
   rows.append({'stream':stream,'symbytes':symbytes,'c13bytes':c13,'module':name,'object':obj})
   o=(no+len(obj.encode())+1+3)&~3
  return rows
 def symbols(self,pe):
  db=self.streams[3];streams=[('global',self.streams[u16(db,20)],0)]
  for m in self.modules():
   if m['stream'] not in (0xffff,0) and m['symbytes']>4:
    streams.append((m['module'],self.streams[m['stream']][:m['symbytes']],4))
  rows=[]
  for module,b,o in streams:
   while o+4<=len(b):
    size=u16(b,o);typ=u16(b,o+2)
    if size<2 or o+2+size>len(b):break
    q=o+4;end=o+2+size;name=None;seg=off=length=0
    if typ==0x110e and size>=12: # S_PUB32
     off=u32(b,q+4);seg=u16(b,q+8);name=cstr(b,q+10)
    elif typ in (0x110f,0x1110,0x1146,0x1147) and size>=37: # PROC32(_ID)
     length=u32(b,q+12);off=u32(b,q+28);seg=u16(b,q+32);name=cstr(b,q+35)
    elif typ in (0x110c,0x110d) and size>=12: # L/GDATA32
     off=u32(b,q+4);seg=u16(b,q+8);name=cstr(b,q+10)
    if name and 0<seg<=len(pe.sections):
     r=pe.sections[seg-1]['rva']+off
     rows.append({'name':name,'rva':hex(r),'size':length,'record':hex(typ),'module':module})
    o=end
  return rows
if __name__=='__main__':
 OUT.mkdir(parents=True, exist_ok=True)
 inventory=[]
 for p in ROOT.rglob('*.dll'):
  pe=PE(p);r={'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(pe.b).hexdigest(),'size':len(pe.b),'image_base':hex(pe.base),'sections':pe.sections,'versions':pe.versions(),'debug':pe.debug(),'exports':pe.exports(),'imports':pe.imports(),'delay_imports':pe.delay_imports()};inventory.append(r)
  if p.name in ['SkyrimUpscaler.dll','PDPerfPlugin.dll','Upscaling.dll','sl.dlss_nr.dll']:
   tag=('skyrim_' if 'skyrim' in str(p) else 'fallout_')+p.stem
   (OUT/(tag+'_strings.json')).write_text(json.dumps(pe.strings(),indent=2))
  print(r['path'],r['sha256'][:16],r['versions'],'exports',len(r['exports']))
 (OUT/'pe_inventory.json').write_text(json.dumps(inventory,indent=2))
 p=ROOT/'fallout/F4SE/Plugins/Upscaling.pdb';pdb=PDB(p);pe=PE(p.with_suffix('.dll'));identity=pdb.identity();identity['pe_codeview']=pe.debug();identity['matches']=any(x['guid']==identity['guid'] and x['age']==identity['age'] for x in pe.debug())
 (OUT/'fallout_pdb_identity.json').write_text(json.dumps(identity,indent=2));(OUT/'fallout_pdb_modules.json').write_text(json.dumps(pdb.modules(),indent=2))
 syms=pdb.symbols(pe);(OUT/'fallout_pdb_symbols.json').write_text(json.dumps(syms,indent=2))
 print('PDB:',identity,'symbols',len(syms));
 with (OUT/'fallout_functions.tsv').open('w') as f:
  w=csv.DictWriter(f,fieldnames=['name','rva','size','record','module'],delimiter='\t');w.writeheader();w.writerows(x for x in syms if x['size'])
