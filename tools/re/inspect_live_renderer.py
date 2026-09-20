"""Bounded read-only inspection of the selected, exact Skyrim 1.6.1170 process.
No remote calls, writes, thread creation, suspension or patching.
"""
import argparse,ctypes,hashlib,json,struct
from pathlib import Path
from ctypes import wintypes as W
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--base',type=lambda x:int(x,0),required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
k.CloseHandle.argtypes=[W.HANDLE];k.CloseHandle.restype=W.BOOL
k.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,ctypes.POINTER(W.DWORD)];k.QueryFullProcessImageNameW.restype=W.BOOL
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)];k.ReadProcessMemory.restype=W.BOOL
h=k.OpenProcess(0x1010,False,a.pid)
if not h:raise ctypes.WinError(ctypes.get_last_error())
try:
 name=ctypes.create_unicode_buffer(32768);n=W.DWORD(len(name))
 if not k.QueryFullProcessImageNameW(h,0,name,ctypes.byref(n)):raise ctypes.WinError(ctypes.get_last_error())
 sha=hashlib.sha256(Path(name.value).read_bytes()).hexdigest()
 if sha!='c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9':raise ValueError('Unknown executable hash')
 def read(address,size):
  if not 0<size<=0x4000:raise ValueError('Read extent exceeded')
  b=ctypes.create_string_buffer(size);got=ctypes.c_size_t()
  if not k.ReadProcessMemory(h,address,b,size,ctypes.byref(got)) or got.value!=size:raise ctypes.WinError(ctypes.get_last_error())
  return b.raw
 if read(a.base,2)!=b'MZ':raise ValueError('Base is not PE')
 a.output.mkdir(parents=True,exist_ok=True)
 report={'process_path':name.value,'sha256':sha,'base':hex(a.base),'pid':a.pid,'regions':{}}
 for label,rva,size in [('renderer_lock',0xe44550,32),('renderer_unlock',0xe44570,32),('renderer_begin',0xe44590,480),('renderer_end',0xe44770,512),('jitter_pair_first',0xe01ac0,0x100),('camera_source_pair_first',0xe01c90,0x240),('jitter_pair_second',0xe58a10,0x840),('camera_source_pair_second',0xe58b80,0x240),('pre_ui_candidate',0xfa4f00,0x1000),('ui_entry_claim_candidate',0xfa3dc0,0x400),('post_world_call',0xfc32e0,0x1000),('renderer',0x32887c0,0x2840),('renderer_data_ptr',0x3286a08,8)]:
  data=read(a.base+rva,size);(a.output/(label+'.bin')).write_bytes(data);report['regions'][label]={'rva':hex(rva),'size':size}
 (a.output/'manifest.json').write_text(json.dumps(report,indent=2))
 print(json.dumps(report))
finally:k.CloseHandle(h)
