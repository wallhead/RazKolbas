"""Read-only call/string cross references from objdump and matching PE/PDB.
This is a static aid, not a decompiler or runtime execution trace.
"""
import sys, json, re, pathlib, bisect, os
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from binary_audit import PE
ROOT=pathlib.Path(os.environ.get('UPSCALER_WORK', '/mnt/data/upscaler_work'));OUT=pathlib.Path(os.environ.get('UPSCALER_AUDIT_OUT', '/mnt/data/upscaler_raw_audit'))
class Trace:
 def __init__(self,pepath,asmfile,symbols=None):
  self.pe=PE(pepath);self.base=self.pe.base;self.syms={};self.ranges={}
  if symbols:
   for s in symbols:
    if s.get('size') and 'dtor$' not in s['name'] and not s['name'].startswith(('std::','__','`std','winrt::','spdlog::','fmt::')):
     r=int(s['rva'],16)
     self.syms.setdefault(r,s['name']);self.ranges[r]=r+s['size']
  for a,b in self.pe.functions():self.ranges.setdefault(a,b)
  self.starts=sorted(self.ranges)
  for a in self.pe.exports():self.syms[int(a['rva'],16)]=a['name']
  self.imports={int(s['iat_rva'],16):d['dll']+'!'+s['name'] for d in self.pe.imports()+self.pe.delay_imports() for s in d['symbols']}
  self.strings={int(a['rva'],16):a['text'] for a in self.pe.strings()}
  self.lines=[]
  for line in pathlib.Path(asmfile).read_text(errors='replace').splitlines():
   m=re.match(r'^\s*([0-9a-f]+):\s',line)
   if m:self.lines.append((int(m[1],16)-self.base,line))
 def refs(self,rva,size=None):
  end=rva+size if size else self.ranges.get(rva,rva+256)
  refs=[]
  for addr,line in self.lines:
   if not rva<=addr<end:continue
   m=re.search(r'\b(call|jmp)\s+(?:0x)?([0-9a-f]{8,16})(?:\s|$)',line)
   if m:
    target=int(m[2],16)-self.base
    refs.append({'at':hex(addr),'op':m[1],'target':hex(target),'name':self.syms.get(target,'sub_'+hex(target))})
   m=re.search(r'# 0x([0-9a-f]+)',line)
   if m:
    target=int(m[1],16)-self.base
    if target in self.imports:refs.append({'at':hex(addr),'op':'import','name':self.imports[target]})
    elif target in self.strings:refs.append({'at':hex(addr),'op':'string','target':hex(target),'text':self.strings[target]})
  return {'rva':hex(rva),'end':hex(end),'name':self.syms.get(rva,'sub_'+hex(rva)),'references':refs}
if __name__=='__main__':
 symbols=json.loads((OUT/'fallout_pdb_symbols.json').read_text())
 t=Trace(ROOT/'fallout/F4SE/Plugins/Upscaling.dll',ROOT/'Upscaling.asm',symbols)
 names=['DX12SwapChain::Present','DX12SwapChain::ResizeBuffersInternal','DX12SwapChain::EvaluateD3D12WorkForCurrentFrame','DX12SwapChain::AcquireCommandContext','DX12SwapChain::ResizeENBScene','DX12SwapChain::BeginNativeUI','DX12SwapChain::FenceFrameSlotAfterPresent','Streamline::UpscaleD3D12','Streamline::EnsureD3D12DLSSNROptions','Streamline::GetD3D12DLSSNRPreparation','Streamline::PrepareDirectDLSSNR','Streamline::CheckFeature','Streamline::CheckFeatures','Streamline::Initialize','Streamline::LoadInterposer','Upscaling::Upscale','Upscaling::InstallHooks','Upscaling::LoadSettings','Upscaling::SaveSettings','Upscaling::CaptureNRAfterSRGuides','Upscaling::CaptureNRMotion','Upscaling::EvaluateD3D12DLSS','Upscaling::EnsureNRGuideResources','ReShadeDepth::Initialize','ReShadeDepth::Capture','ReShadeDepth::PublishPresent','ReShadeDepth::FrameGraph::Execute','nvngx::dlss_nr::D3D12Backend::SetEvaluationParameters','nvngx::dlss_nr::D3D12Backend::SetCreationParameters','nvngx::dlss_nr::D3D12Backend::Evaluate','nvngx::dlss_nr::D3D12Backend::EnsureFeature','nvngx::dlss_nr::D3D12Backend::PrepareFeature']
 rows=[t.refs(r) for r,n in t.syms.items() if n in names]
 (OUT/'fallout_rendering_xrefs.json').write_text(json.dumps(rows,indent=2))
 for a in rows:
  if a['name'] in ['DX12SwapChain::Present','DX12SwapChain::ResizeBuffersInternal','Streamline::UpscaleD3D12','Upscaling::Upscale','nvngx::dlss_nr::D3D12Backend::SetEvaluationParameters','Streamline::EnsureD3D12DLSSNROptions']:
   print('\n',a['name'],a['rva'],a['end'])
   for ref in a['references']:
    if ref.get('name','').startswith(('std::','logger::','REX::','sub_')):continue
    print(ref['at'],ref['op'],ref.get('name',ref.get('text','')))
