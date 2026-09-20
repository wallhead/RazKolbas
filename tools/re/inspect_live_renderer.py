"""Bounded read-only inspection of the selected, exact Skyrim 1.6.1170 process.
No remote calls, writes, thread creation, suspension or patching.
"""
import argparse,ctypes,hashlib,json,os,struct
from pathlib import Path
from ctypes import wintypes as W
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--base',type=lambda x:int(x,0),required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--enb-base',type=lambda x:int(x,0));p.add_argument('--reshade-base',type=lambda x:int(x,0));p.add_argument('--system-d3d11-base',type=lambda x:int(x,0));a=p.parse_args()
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
 for label,rva,size in [('drs_control_candidate',0x643c00,0x100),('drs_policy_callee',0xe587f0,0x400),('scissor_candidate',0xe4adf0,0x180),('renderer_lock',0xe44550,32),('renderer_unlock',0xe44570,32),('renderer_begin',0xe44590,480),('renderer_end',0xe44770,512),('renderer_begin_descriptor_copy',0xe4fb90,0x20),('renderer_begin_setup_callee',0xe4f180,0x200),('renderer_resize_buffers',0xe43bc0,0x600),('renderer_resize_target',0xe44050,0x300),('jitter_pair_first',0xe01ac0,0x100),('camera_source_pair_first',0xe01c90,0x240),('jitter_pair_second',0xe58a10,0x840),('camera_source_pair_second',0xe58b80,0x240),('pre_ui_candidate',0xfa4f00,0x1000),('ui_entry_claim_candidate',0xfa3dc0,0x400),('post_world_call',0xfc32e0,0x1000),('world_target_indices',0x202ab70,0x90),('renderer',0x32887c0,0x2840),('graphics_state',0x328cc20,0x1000),('renderer_data_ptr',0x3286a08,8)]:
  data=read(a.base+rva,size);(a.output/(label+'.bin')).write_bytes(data);report['regions'][label]={'rva':hex(rva),'size':size}
 # e449ba in the decoded world-draw function loads this static pointer and
 # calls its vtable +0x108 to bind renderer targets. The prior snapshots
 # started 0x10 bytes too late to identify this exact object/method.
 image_size=0x3870000
 def in_game(address,size):
  return a.base<=address and size<=image_size-(address-a.base)
 enb_size=0xaae000
 enb_hash='47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58'
 enb_path=Path(name.value).with_name('d3d11.dll')
 enb_valid=bool(a.enb_base and enb_path.is_file() and
  hashlib.sha256(enb_path.read_bytes()).hexdigest()==enb_hash and
  read(a.enb_base,2)==b'MZ')
 reshade_size=0x51c000
 reshade_hash='059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc'
 reshade_path=Path(name.value).with_name('dxgi.dll')
 reshade_valid=bool(a.reshade_base and reshade_path.is_file() and
  hashlib.sha256(reshade_path.read_bytes()).hexdigest()==reshade_hash and
  read(a.reshade_base,2)==b'MZ')
 system_d3d11_size=0x257000
 system_d3d11_hash='722871e4ac32972617483197709fe0d924ced5ed894b18fd13e0813d0b25950f'
 system_d3d11_path=Path(os.environ.get('SystemRoot',r'C:\Windows'))/'System32'/'d3d11.dll'
 system_d3d11_valid=bool(a.system_d3d11_base and system_d3d11_path.is_file() and
  hashlib.sha256(system_d3d11_path.read_bytes()).hexdigest()==system_d3d11_hash and
  read(a.system_d3d11_base,2)==b'MZ')
 def owner_of(address,size):
  if in_game(address,size):return 'game',a.base
  if enb_valid and a.enb_base<=address and size<=enb_size-(address-a.enb_base):
   return 'enb',a.enb_base
  if reshade_valid and a.reshade_base<=address and size<=reshade_size-(address-a.reshade_base):
   return 'reshade',a.reshade_base
  if system_d3d11_valid and a.system_d3d11_base<=address and size<=system_d3d11_size-(address-a.system_d3d11_base):
   return 'system_d3d11',a.system_d3d11_base
  return None,None
 owner={'pointer_rva':'0x32887b0'}
 try:
  object_address=struct.unpack('<Q',read(a.base+0x32887b0,8))[0]
  owner['object']=hex(object_address)
  if object_address<0x10000:raise ValueError('Renderer interface is null/invalid')
  table=struct.unpack('<Q',read(object_address,8))[0]
  owner['vtable']=hex(table)
  module,module_base=owner_of(table,0x1b0)
  if not module:raise ValueError('Renderer vtable is outside verified game/ENB images')
  entries=read(table,0x1b0)
  (a.output/'renderer_interface_vtable.bin').write_bytes(entries)
  owner['vtable_owner']=module
  owner['vtable_rva']=hex(table-module_base)
  for label,slot in [('bind_targets',0x108),('clear_targets',0x190)]:
   method=struct.unpack_from('<Q',entries,slot)[0]
   owner[label+'_method']=hex(method)
   method_module,method_base=owner_of(method,0x300)
   if not method_module:
    owner[label+'_error']='Method is outside verified game/ENB images'
    continue
   (a.output/('renderer_'+label+'_method.bin')).write_bytes(read(method,0x300))
   owner[label+'_method_owner']=method_module
   owner[label+'_method_rva']=hex(method-method_base)
  # Exact ENB build 47ff... delegates bind_targets through [object+0x6c68]
  # (RVA 0x69227) and invokes the underlying vtable slot +0x108.
  if module=='enb':
   underlying=struct.unpack('<Q',read(object_address+0x6c68,8))[0]
   owner['underlying_object']=hex(underlying)
   if underlying<0x10000:raise ValueError('Underlying renderer is null/invalid')
   underlying_table=struct.unpack('<Q',read(underlying,8))[0]
   owner['underlying_vtable']=hex(underlying_table)
   underlying_module,underlying_base=owner_of(underlying_table,0x1b0)
   if not underlying_module:
    raise ValueError('Underlying renderer vtable is outside verified images')
   underlying_entries=read(underlying_table,0x1b0)
   (a.output/'underlying_renderer_vtable.bin').write_bytes(underlying_entries)
   owner['underlying_vtable_owner']=underlying_module
   owner['underlying_vtable_rva']=hex(underlying_table-underlying_base)
   for label,slot in [('bind_targets',0x108),('clear_targets',0x190)]:
    method=struct.unpack_from('<Q',underlying_entries,slot)[0]
    owner['underlying_'+label+'_method']=hex(method)
    method_module,method_base=owner_of(method,0x400)
    if not method_module:
     owner['underlying_'+label+'_error']='Method is outside verified images'
     continue
    (a.output/('underlying_renderer_'+label+'_method.bin')).write_bytes(read(method,0x400))
    owner['underlying_'+label+'_method_owner']=method_module
    owner['underlying_'+label+'_method_rva']=hex(method-method_base)
   # Exact ReShade build 0591... forwards D3D11 context slot +0x108
   # through [object+0x18] at RVA 0xf40e9. Only follow this verified wrapper.
   if underlying_module=='reshade':
    delegate=struct.unpack('<Q',read(underlying+0x18,8))[0]
    owner['reshade_delegate_object']=hex(delegate)
    if delegate<0x10000:raise ValueError('ReShade delegate is null/invalid')
    delegate_table=struct.unpack('<Q',read(delegate,8))[0]
    owner['reshade_delegate_vtable']=hex(delegate_table)
    delegate_entries=read(delegate_table,0x1b0)
    (a.output/'reshade_delegate_vtable.bin').write_bytes(delegate_entries)
    table_module,table_base=owner_of(delegate_table,0x1b0)
    owner['reshade_delegate_vtable_owner']=table_module or 'heap'
    if table_module:owner['reshade_delegate_vtable_rva']=hex(delegate_table-table_base)
    for label,slot in [('bind_targets',0x108),('clear_targets',0x190)]:
     method=struct.unpack_from('<Q',delegate_entries,slot)[0]
     owner['reshade_delegate_'+label+'_method']=hex(method)
     method_module,method_base=owner_of(method,0x1000)
     owner['reshade_delegate_'+label+'_method_owner']=method_module or 'unknown'
     if not method_module:
      owner['reshade_delegate_'+label+'_error']='Method is outside verified images'
      continue
     (a.output/('reshade_delegate_'+label+'_method.bin')).write_bytes(read(method,0x1000))
     owner['reshade_delegate_'+label+'_method_rva']=hex(method-method_base)
 except (OSError,ValueError) as error:
  owner['error']=str(error)
 report['renderer_interface']=owner
 (a.output/'manifest.json').write_text(json.dumps(report,indent=2))
 print(json.dumps(report))
finally:k.CloseHandle(h)
