"""Hash-gated read-only resolution of occupied Skyrim renderer hook entries.

Run only against a user-started exact 1.6.1170 process. No remote code,
thread, breakpoint, memory write, input event, or process-control operation.
"""
import argparse
import ctypes
import hashlib
import json
import struct
import time
from ctypes import wintypes as W
from pathlib import Path

GAME_SHA256 = "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9"
SITES = {"target_creation_wrapper": 0xe4fbb0, "resize_buffer_entry": 0xe43e84}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--base", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=12)
    parser.add_argument("--interval", type=float, default=1.0)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Output report already exists")
    if not 1 <= args.samples <= 30 or not 0 <= args.interval <= 5:
        raise ValueError("Sampling bound exceeded")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, ctypes.POINTER(W.DWORD)]
    kernel.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                          ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    psapi.EnumProcessModulesEx.argtypes = [W.HANDLE, ctypes.POINTER(W.HMODULE),
                                           W.DWORD, ctypes.POINTER(W.DWORD), W.DWORD]
    psapi.GetModuleInformation.argtypes = [W.HANDLE, W.HMODULE, ctypes.c_void_p, W.DWORD]
    psapi.GetModuleFileNameExW.argtypes = [W.HANDLE, W.HMODULE, W.LPWSTR, W.DWORD]
    class ModuleInfo(ctypes.Structure):
        _fields_ = [("base", ctypes.c_void_p), ("size", W.DWORD),
                    ("entry", ctypes.c_void_p)]
    class MemoryInfo(ctypes.Structure):
        _fields_ = [("base", ctypes.c_void_p), ("allocationBase", ctypes.c_void_p),
                    ("allocationProtect", W.DWORD), ("_pad", W.DWORD),
                    ("size", ctypes.c_size_t), ("state", W.DWORD),
                    ("protect", W.DWORD), ("type", W.DWORD), ("_pad2", W.DWORD)]
    kernel.VirtualQueryEx.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                      ctypes.c_size_t]
    kernel.VirtualQueryEx.restype = ctypes.c_size_t
    handle = kernel.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        name = ctypes.create_unicode_buffer(32768)
        length = W.DWORD(len(name))
        if not kernel.QueryFullProcessImageNameW(handle, 0, name, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        path = Path(name.value)
        if hashlib.sha256(path.read_bytes()).hexdigest() != GAME_SHA256:
            raise ValueError("Unknown Skyrim executable hash")

        def read(address, size):
            if not 0 < size <= 0x120:
                raise ValueError("Read extent exceeded")
            buffer = ctypes.create_string_buffer(size)
            copied = ctypes.c_size_t()
            if not kernel.ReadProcessMemory(handle, address, buffer, size,
                                            ctypes.byref(copied)) or copied.value != size:
                raise ctypes.WinError(ctypes.get_last_error())
            return buffer.raw

        if read(args.base, 2) != b"MZ":
            raise ValueError("Base is not a PE image")
        module_handles = (W.HMODULE * 2048)()
        needed = W.DWORD()
        if not psapi.EnumProcessModulesEx(handle, module_handles,
                ctypes.sizeof(module_handles), ctypes.byref(needed), 0x03):
            raise ctypes.WinError(ctypes.get_last_error())
        if needed.value > ctypes.sizeof(module_handles):
            raise ValueError("Module list exceeds bounded buffer")
        modules = []
        for module in module_handles[:needed.value // ctypes.sizeof(W.HMODULE)]:
            info = ModuleInfo()
            if not psapi.GetModuleInformation(handle, module, ctypes.byref(info),
                                              ctypes.sizeof(info)):
                continue
            module_path = ctypes.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(handle, module, module_path,
                                              len(module_path)):
                continue
            modules.append({"base": info.base, "end": info.base + info.size,
                            "path": module_path.value})
        sites = {}
        for label, rva in SITES.items():
            entry = args.base + rva
            data = read(entry, 16)
            item = {"rva": hex(rva), "entryBytes": data.hex()}
            try:
                if data[:2] == b"\xff\x25":
                    pointer_address = entry + 6 + struct.unpack_from("<i", data, 2)[0]
                    item["pointerAddress"] = hex(pointer_address)
                    pointer = struct.unpack("<Q", read(pointer_address, 8))[0]
                    item["target"] = hex(pointer)
                    item["targetBytes"] = read(pointer, 96).hex()
                elif data[:1] == b"\xe9":
                    target = entry + 5 + struct.unpack_from("<i", data, 1)[0]
                    item["target"] = hex(target)
                    item["targetBytes"] = read(target, 96).hex()
                if "target" in item:
                    resolved = int(item["target"], 16)
                    owner = next((module for module in modules
                                  if module["base"] <= resolved < module["end"]), None)
                    item["targetOwner"] = owner["path"] if owner else None
                    region = MemoryInfo()
                    if kernel.VirtualQueryEx(handle, resolved, ctypes.byref(region),
                                             ctypes.sizeof(region)) == ctypes.sizeof(region):
                        item["targetRegion"] = {"base": hex(region.base),
                            "allocationBase": hex(region.allocationBase),
                            "size": hex(region.size), "state": hex(region.state),
                            "protect": hex(region.protect), "type": hex(region.type)}
                    if label == "resize_buffer_entry" and item["targetBytes"].startswith("488b1526000000"):
                        data_pointer = struct.unpack("<Q", read(resolved + 0x2d, 8))[0]
                        data_owner = next((module for module in modules
                            if module["base"] <= data_pointer < module["end"]), None)
                        item["embeddedDataPointer"] = hex(data_pointer)
                        item["embeddedDataOwner"] = data_owner["path"] if data_owner else None
            except OSError as error:
                item["followError"] = str(error)
            sites[label] = item
        samples = []
        for index in range(args.samples):
            state = read(args.base + 0x328cc20, 0x120)
            viewport = struct.unpack("<6f", read(args.base + 0x202abe0, 24))
            samples.append({"index": index,
                "display": struct.unpack_from("<II", state, 0x24),
                "currentRatio": struct.unpack_from("<ff", state, 0x104),
                "previousRatio": struct.unpack_from("<ff", state, 0x10c),
                "drsCounter": struct.unpack_from("<I", state, 0x118)[0],
                "drsDirectionFlags": list(state[0x11c:0x11e]),
                "dynamicResolutionEnabled": read(args.base + 0x2012490, 1)[0],
                "viewport": viewport})
            if index + 1 < args.samples:
                time.sleep(args.interval)
        report = {"processPath": str(path), "gameSha256": GAME_SHA256,
                  "pid": args.pid, "base": hex(args.base), "sites": sites,
                  "samples": samples}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report))
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    main()
