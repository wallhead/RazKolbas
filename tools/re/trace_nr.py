"""Hash-gated, read-only native NR static trace. Requires capstone==5.0.6.

Never loads reference hosts or vendor DLLs. Outputs instruction evidence, not a recovered ABI assertion.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'artifacts/local/python'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_REG_RIP

spec = importlib.util.spec_from_file_location('audit', ROOT / 'RazKolbas_Codex_Handoff/tools/re/binary_audit.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    package = args.reference_root / 'Upscaling Custom 108772 1.65 2026-09-13T17-12Z DzMlEf9Og/F4SE/Plugins'
    dll = package / 'Upscaling.dll'
    expected = 'f2633bcf11f618569eac77d1c7e6bf136a498fdb53f5eb1f205bfbb3a74cac7b'
    actual = hashlib.sha256(dll.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError('HASH_MISMATCH: Upscaling.dll')
    pe, pdb = audit.PE(dll), audit.PDB(package / 'Upscaling.pdb')
    identity = pdb.identity()
    if identity['guid'] != '7057e107-1a03-4c53-ab14-7cd63ad16f3e' or identity['age'] != 71:
        raise ValueError('PDB_IDENTITY_MISMATCH')
    if not any(cv['guid'] == identity['guid'] and cv['age'] == identity['age'] for cv in pe.debug()):
        raise ValueError('PE_PDB_MISMATCH')
    symbols = pdb.symbols(pe)
    names = {int(s['rva'], 16): s['name'] for s in symbols}
    functions = dict(pe.functions())
    imports = {int(s['iat_rva'], 16): d['dll'] + '!' + s['name'] for d in pe.imports() + pe.delay_imports() for s in d['symbols']}
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    selected = {}
    for symbol in symbols:
        name = symbol['name']
        if name.startswith('nvngx::dlss_nr::') or name in ('Streamline::PrepareDirectDLSSNR', 'Streamline::GetD3D12DLSSNRPreparation') or 'NVSDK_NGX_GetModuleFileNameW_Proxy' in name or name.endswith(('::WritePointer', '::AllocateNRResource', '::ReleaseNRResource', '::NVSDK_NGX_DLSSNR_ComputeScalingRatio')):
            start = int(symbol['rva'], 16)
            if start in functions or symbol.get('size', 0) > 0:
                selected[start] = name
                # .pdata can split one C++ function into chained unwind regions.
                # The matched PDB PROC record is the complete function extent.
                functions[start] = max(functions.get(start, start), start + symbol.get('size', 0))
    args.output.mkdir(parents=True, exist_ok=True)
    index = []
    for start, name in sorted(selected.items()):
        end = functions[start]
        lines = [f'; STATIC_OBSERVED {name}', f'; DLL SHA256 {actual}', f'; RVA {start:#x}..{end:#x} PDB {identity["guid"]} age {identity["age"]}']
        for insn in decoder.disasm(pe.b[pe.off(start):pe.off(start) + end - start], pe.base + start):
            notes = []
            for operand in insn.operands:
                target = None
                if operand.type == X86_OP_MEM and operand.mem.base == X86_REG_RIP:
                    target = insn.address + insn.size + operand.mem.disp - pe.base
                elif operand.type == X86_OP_IMM and insn.mnemonic in ('call', 'jmp'):
                    target = operand.imm - pe.base
                if target is None:
                    continue
                if target in imports:
                    notes.append(imports[target])
                elif target in names and not names[target].startswith('??_C@'):
                    notes.append(names[target])
                else:
                    try:
                        offset = pe.off(target)
                        raw = pe.b[offset:offset+256].split(b'\0')[0]
                        if 3 <= len(raw) < 256 and all(32 <= byte < 127 for byte in raw):
                            notes.append(repr(raw.decode('ascii')))
                    except (ValueError, IndexError):
                        pass
            lines.append(f'{insn.address-pe.base:08x}  {insn.bytes.hex(" "):32} {insn.mnemonic:8} {insn.op_str}' + (' ; ' + ' | '.join(notes) if notes else ''))
        filename = f'{start:08x}.asm'
        (args.output / filename).write_text('\n'.join(lines) + '\n', encoding='utf-8')
        index.append({'name': name, 'rva': hex(start), 'end': hex(end), 'file': filename})
    (args.output / 'index.json').write_text(json.dumps({'dll_sha256': actual, 'pdb': identity, 'functions': index}, indent=2), encoding='utf-8')
    print(json.dumps(index, indent=2))


if __name__ == '__main__':
    main()
