"""Find candidate fixed-offset accesses in a captured decoded Skyrim .text.

Offline triage only: x86 instructions can overlap, so each result requires
function-boundary/caller verification before it is treated as a patch site.
"""
import argparse
import json
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, Cs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--text", type=Path, required=True)
    parser.add_argument("--rva", type=lambda value: int(value, 0), default=0x1000)
    parser.add_argument("--offset", type=lambda value: int(value, 0), default=0x118)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Output already exists")
    code = args.text.read_bytes()
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    if not 0 <= args.offset <= 0x7fffffff:
        raise ValueError("Offset must be a positive disp32")
    needle = args.offset.to_bytes(4, "little")
    results = {}
    position = code.find(needle)
    while position >= 0:
        for start in range(max(0, position - 14), position + 1):
            instruction = next(decoder.disasm(code[start:position + 15],
                                               args.rva + start, count=1), None)
            if not instruction or instruction.address + instruction.size < args.rva + position + 4:
                continue
            if not any(op.type == CS_OP_MEM and op.mem.disp == args.offset
                       for op in instruction.operands):
                continue
            writes = bool(instruction.operands and instruction.operands[0].type == CS_OP_MEM
                          and instruction.operands[0].mem.disp == args.offset
                          and instruction.mnemonic not in ("cmp", "test"))
            results[instruction.address] = {
                "rva": hex(instruction.address), "bytes": instruction.bytes.hex(),
                "mnemonic": instruction.mnemonic, "operands": instruction.op_str,
                "possibleWrite": writes}
        position = code.find(needle, position + 1)
    ordered = [results[address] for address in sorted(results)]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(ordered, indent=2), encoding="utf-8")
    print(json.dumps({"candidates": len(ordered), "possibleWrites": sum(x["possibleWrite"] for x in ordered),
                      "output": str(args.output)}))


if __name__ == "__main__":
    main()
