"""Isolated, exact-driver NGX parameter ABI experiment; never creates a feature.

Static basis: _nvngx.dll allocator RVA 0x67f50, destroy RVA 0x68030,
constructor RVA 0x66b40 and vtable RVA 0xb59b8. See docs/re/NR_BOOTSTRAP.md.
The driver module remains loaded until child process exit (no core Init call).
"""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import subprocess
import sys

CORE_SHA256 = "91d3742f0df3f2dd85141fa39ea257f7b9243c3e6755cd7fea894c721e7491aa"
SLOTS = {0: 0x37A0, 3: 0x2F30, 4: 0x2EB0, 6: 0x2E70,
         8: 0x3800, 11: 0x3310, 12: 0x3290, 14: 0x3090, 16: 0x3390}


def emit(**values):
    print(json.dumps(values), flush=True)


def child(path):
    if sys.platform != "win32" or c.sizeof(c.c_void_p) != 8:
        raise RuntimeError("WINDOWS_X64_REQUIRED")
    # Keep an open, non-write/non-delete-shared file handle through loading.
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [c.c_wchar_p, c.c_uint32, c.c_uint32, c.c_void_p,
                                  c.c_uint32, c.c_uint32, c.c_void_p]
    kernel.CreateFileW.restype = c.c_void_p
    kernel.CloseHandle.argtypes = [c.c_void_p]
    handle = kernel.CreateFileW(str(path), 0x80000000, 1, None, 3, 0x80, None)
    if handle == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    try:
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != CORE_SHA256:
            raise RuntimeError("CORE_HASH_MISMATCH: " + actual)
        core = c.WinDLL(str(path), winmode=0x100 | 0x800)
    finally:
        kernel.CloseHandle(handle)
    emit(core=str(path), sha256=actual, core_init="NOT CALLED")
    allocate = core.NVSDK_NGX_D3D12_AllocateParameters
    allocate.argtypes, allocate.restype = [c.POINTER(c.c_void_p)], c.c_uint32
    destroy = core.NVSDK_NGX_D3D12_DestroyParameters
    destroy.argtypes, destroy.restype = [c.c_void_p], c.c_uint32
    if (c.cast(allocate, c.c_void_p).value - core._handle != 0x67F50 or
            c.cast(destroy, c.c_void_p).value - core._handle != 0x68030):
        raise RuntimeError("EXPORT_RVA_MISMATCH")
    parameter = c.c_void_p()
    result = allocate(c.byref(parameter))
    emit(allocate_result=hex(result), parameter_nonnull=bool(parameter.value))
    if result != 1 or not parameter.value:
        return 3
    passed = False
    try:
        table_address = c.c_void_p.from_address(parameter.value).value
        if table_address != core._handle + 0xB59B8:
            raise RuntimeError("VTABLE_ADDRESS_MISMATCH")
        table = (c.c_void_p * 17).from_address(table_address)
        for slot, rva in SLOTS.items():
            if table[slot] != core._handle + rva:
                raise RuntimeError(f"VTABLE_SLOT_MISMATCH: {slot}")
        # Each Set/Get pair is independently verified against the exact driver
        # vtable and disassembly; no reference host object is instantiated.
        marker = c.c_uint32(0x524B)
        cases = [("uint32", 4, 12, c.c_uint32, 0xF1234567),
                 ("int32", 3, 11, c.c_int32, -12345),
                 ("float", 6, 14, c.c_float, 0.375),
                 ("pointer", 0, 8, c.c_void_p, c.addressof(marker))]
        for name, setter, getter, scalar, value in cases:
            key = ("RazKolbas.Probe." + name).encode("ascii")
            put = c.WINFUNCTYPE(None, c.c_void_p, c.c_char_p, scalar)(table[setter])
            get = c.WINFUNCTYPE(c.c_uint32, c.c_void_p, c.c_char_p,
                               c.POINTER(scalar))(table[getter])
            put(parameter, key, value)
            output = scalar()
            result = get(parameter, key, c.byref(output))
            same = output.value == value
            emit(case=name, get_result=hex(result), roundtrip=same)
            if result != 1 or not same:
                raise RuntimeError("PARAMETER_ROUNDTRIP_FAILED: " + name)
        c.WINFUNCTYPE(None, c.c_void_p)(table[16])(parameter)
        output = c.c_uint32()
        get_uint = c.WINFUNCTYPE(c.c_uint32, c.c_void_p, c.c_char_p,
                                c.POINTER(c.c_uint32))(table[12])
        missing = get_uint(parameter, b"RazKolbas.Probe.uint32", c.byref(output))
        emit(after_reset_get_result=hex(missing))
        if (missing & 0xFFF00000) != 0xBAD00000:
            raise RuntimeError("RESET_DID_NOT_REMOVE_PARAMETER")
        passed = True
    finally:
        released = destroy(parameter)
        emit(destroy_result=hex(released))
        if released != 1:
            passed = False
    emit(feature_create="NOT RUN", nr_parameter_consumption="NOT RUN")
    return 0 if passed else 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core", type=Path)
    parser.add_argument("--child", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.child:
        return child(args.core.resolve(strict=True))
    # A crash, hung driver or incomplete retirement is confined to this child.
    try:
        result = subprocess.run([sys.executable, "-u", str(Path(__file__).resolve()),
                                 str(args.core.resolve(strict=True)), "--child"],
                                capture_output=True, text=True, timeout=30)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        emit(child_exit=result.returncode)
        return 0 if result.returncode == 0 else 5
    except subprocess.TimeoutExpired as error:
        emit(child_timeout_seconds=30)
        if error.stdout:
            print(error.stdout.decode(errors="replace"), end="")
        return 6


if __name__ == "__main__":
    sys.exit(main())
