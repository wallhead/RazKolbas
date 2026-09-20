"""Read-only capture of the exact Skyrim 1.6.1170 process's decoded code.

The disk image is encrypted at the DRS and renderer functions. The resulting
raw code is local RE evidence under an ignored output directory, never a
product input or staged repository artifact. No remote call, thread, patch,
or input event is sent to the game.
"""
import argparse
import ctypes
import hashlib
import json
from ctypes import wintypes as W
from pathlib import Path

import pefile

GAME_SHA256 = "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--base", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Output directory already exists")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, ctypes.POINTER(W.DWORD)]
    kernel.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                          ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    handle = kernel.OpenProcess(0x1010, False, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        name = ctypes.create_unicode_buffer(32768)
        length = W.DWORD(len(name))
        if not kernel.QueryFullProcessImageNameW(handle, 0, name, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        game_path = Path(name.value)
        if hashlib.sha256(game_path.read_bytes()).hexdigest() != GAME_SHA256:
            raise ValueError("Unknown Skyrim executable hash")

        def read(address, size):
            if not 0 < size <= 0x4000:
                raise ValueError("Read extent exceeded")
            buffer = ctypes.create_string_buffer(size)
            copied = ctypes.c_size_t()
            if not kernel.ReadProcessMemory(handle, address, buffer, size,
                                            ctypes.byref(copied)) or copied.value != size:
                raise ctypes.WinError(ctypes.get_last_error())
            return buffer.raw

        if read(args.base, 2) != b"MZ":
            raise ValueError("Base is not a PE image")
        pe = pefile.PE(str(game_path), fast_load=True)
        sections = [(section.VirtualAddress, section.Misc_VirtualSize)
                    for section in pe.sections if section.Name.rstrip(b"\0") == b".text"
                    and section.Characteristics & 0x20000000]
        if not sections or not any(rva <= 0xe587f0 < rva + size for rva, size in sections):
            raise ValueError("Expected decoded DRS code is outside executable sections")
        if read(args.base + 0xe587f0, 7) != bytes.fromhex("83b91801000000"):
            raise ValueError("Expected DRS callee is not decoded")
        args.output.mkdir(parents=True)
        report = {"processPath": str(game_path), "gameSha256": GAME_SHA256,
                  "pid": args.pid, "base": hex(args.base), "sections": []}
        for rva, size in sections:
            data = bytearray()
            for offset in range(0, size, 0x4000):
                data.extend(read(args.base + rva + offset, min(0x4000, size - offset)))
            filename = f"text_{rva:x}.bin"
            (args.output / filename).write_bytes(data)
            report["sections"].append({"rva": hex(rva), "size": size,
                                       "file": filename,
                                       "sha256": hashlib.sha256(data).hexdigest()})
        (args.output / "manifest.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report))
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    main()
