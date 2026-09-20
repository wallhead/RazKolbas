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
 for label,rva,size in [('drs_control_candidate',0x643c00,0x100),('drs_policy_callee',0xe587f0,0x400),('scissor_candidate',0xe4adf0,0x180),('renderer_lock',0xe44550,32),('renderer_unlock',0xe44570,32),('renderer_begin',0xe44590,480),('renderer_end',0xe44770,512),('renderer_begin_descriptor_copy',0xe4fb90,0x20),('renderer_begin_setup_callee',0xe4f180,0x200),('renderer_resize_buffers',0xe43bc0,0x600),('renderer_resize_target',0xe44050,0x300),('jitter_pair_first',0xe01ac0,0x100),('camera_source_pair_first',0xe01c90,0x240),('jitter_pair_second',0xe58a10,0x840),('camera_source_pair_second',0xe58b80,0x240),('pre_ui_candidate',0xfa4f00,0x1000),('ui_entry_claim_candidate',0xfa3dc0,0x400),('post_world_call',0xfc32e0,0x1000),('renderer',0x32887c0,0x2840),('graphics_state',0x328cc20,0x1000),('renderer_data_ptr',0x3286a08,8)]:
  data=read(a.base+rva,size);(a.output/(label+'.bin')).write_bytes(data);report['regions'][label]={'rva':hex(rva),'size':size}
 # e449ba in the decoded world-draw function loads this static pointer and
 # calls its vtable +0x108 to bind renderer targets. The prior snapshots
 # started 0x10 bytes too late to identify this exact object/method.
 image_size=0x3870000
 def in_game(address,size):
  return a.base<=address and size<=image_size-(address-a.base)
 owner={'pointer_rva':'0x32887b0'}
 try:
  object_address=struct.unpack('<Q',read(a.base+0x32887b0,8))[0]
  owner['object']=hex(object_address)
  if object_address<0x10000:raise ValueError('Renderer interface is null/invalid')
  table=struct.unpack('<Q',read(object_address,8))[0]
  owner['vtable']=hex(table)
  if not in_game(table,0x1b0):raise ValueError('Renderer vtable is not in verified game image')
  entries=read(table,0x1b0)
  (a.output/'renderer_interface_vtable.bin').write_bytes(entries)
  owner['vtable_rva']=hex(table-a.base)
  for label,slot in [('bind_targets',0x108),('clear_targets',0x190)]:
   method=struct.unpack_from('<Q',entries,slot)[0]
   owner[label+'_method']=hex(method)
   if not in_game(method,0x300):
    owner[label+'_error']='Method is outside verified game image'
    continue
   (a.output/('renderer_'+label+'_method.bin')).write_bytes(read(method,0x300))
   owner[label+'_method_rva']=hex(method-a.base)
 except (OSError,ValueError) as error:
  owner['error']=str(error)
 report['renderer_interface']=owner
 (a.output/'manifest.json').write_text(json.dumps(report,indent=2))
 print(json.dumps(report))
finally:k.CloseHandle(h)
