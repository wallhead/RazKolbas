"""Read-only, exact-hash SR reference disassembly; never loads either host DLL."""
import argparse
import hashlib
import json
from pathlib import Path
from trace_nr import audit, Cs, CS_ARCH_X86, CS_MODE_64, X86_OP_MEM, X86_OP_IMM, X86_REG_RIP

PROFILES = {
    'pd': ('UpscalerBasePlugin/PDPerfPlugin.dll', '8ef7dc27fafbb89ba4b0ea46d16b749fb8b02f512e211976dc1929c915ed324d'),
    'skyrim': ('SKSE/Plugins/SkyrimUpscaler.dll', '94ded937705c721be5aba784cbb04f5c3873acf2ae477b5727f1b40b00018dcb'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--host', choices=PROFILES, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rva', type=lambda x: int(x, 0), action='append')
    args = parser.parse_args()
    relative, expected = PROFILES[args.host]
    path = args.package / relative
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('HASH_MISMATCH')
    pe = audit.PE(path)
    names = {int(e['rva'], 16): e['name'] for e in pe.exports()}
    names.update({int(s['iat_rva'], 16): d['dll'] + '!' + s['name']
                  for d in pe.imports() + pe.delay_imports() for s in d['symbols']})
    strings = {int(s['rva'], 16): s['text'] for s in pe.strings()}
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    args.output.mkdir(parents=True, exist_ok=True)
    index = []
    for start, end in pe.functions():
        if args.rva and not any(start <= rva < end for rva in args.rva):
            continue
        lines = [f'; STATIC_OBSERVED RVA {start:#x}..{end:#x}; SHA256 {expected}']
        refs = []
        for insn in decoder.disasm(pe.b[pe.off(start):pe.off(start)+end-start], pe.base+start):
            notes = []
            for operand in insn.operands:
                target = None
                if operand.type == X86_OP_MEM and operand.mem.base == X86_REG_RIP:
                    target = insn.address + insn.size + operand.mem.disp - pe.base
                elif operand.type == X86_OP_IMM and insn.mnemonic in ('call', 'jmp'):
                    target = operand.imm - pe.base
                if target in names:
                    notes.append(names[target])
                if target in strings:
                    notes.append(repr(strings[target]))
                if target is not None:
                    refs.append({'at': hex(insn.address-pe.base), 'target': hex(target), 'names': notes})
            lines.append(f'{insn.address-pe.base:08x}  {insn.bytes.hex(" "):32} {insn.mnemonic:8} {insn.op_str}' + (' ; ' + ' | '.join(notes) if notes else ''))
        filename = f'{start:08x}.asm'
        (args.output / filename).write_text('\n'.join(lines)+'\n', encoding='utf-8')
        index.append({'rva': hex(start), 'end': hex(end), 'name': names.get(start), 'file': filename, 'references': refs})
    (args.output / 'index.json').write_text(json.dumps({'sha256': expected, 'functions': index}, indent=2), encoding='utf-8')
    print(f'{len(index)} hash-verified function regions written to {args.output}')


if __name__ == '__main__':
    main()
