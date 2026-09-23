"""Read-only ADB capability inventory and a screencap capture candidate.

No game decision is read from ADB metadata. ADB is only used for connection,
environment facts and screen pixels. Touch injection is intentionally absent
until a backend passes independent-contact tests on a real emulator.
"""

import binascii
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

from .clock import Clock, HostClock
from .contracts import Frame


def find_adb() -> str | None:
    direct = shutil.which("adb")
    if direct:
        return direct
    for variable in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        root = os.environ.get(variable)
        if root:
            candidate = Path(root) / "platform-tools" / "adb.exe"
            if candidate.is_file():
                return str(candidate)
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        candidate = Path(local_app_data) / "Android" / "Sdk" / "platform-tools" / "adb.exe"
        if candidate.is_file():
            return str(candidate)
    return None


def adb_call(adb: str, args: list[str], timeout_s: float = 10) -> bytes:
    result = subprocess.run([adb, *args], capture_output=True, timeout=timeout_s,
                            check=False)
    if result.returncode:
        error = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"adb failed ({result.returncode}): {error}")
    return result.stdout


def list_devices(adb: str) -> list[dict[str, str]]:
    lines = adb_call(adb, ["devices", "-l"]).decode(errors="replace").splitlines()
    devices = []
    for line in lines[1:]:
        parts = line.split()
        if len(parts) >= 2:
            devices.append({"serial": parts[0], "state": parts[1],
                            "details": " ".join(parts[2:])})
    return devices


def probe(adb: str, serial: str | None = None) -> dict:
    devices = list_devices(adb)
    report: dict = {"adb_path": adb, "devices": devices,
                    "capture_candidate": "adb exec-out screencap -p (PNG, full frame)",
                    "touch_candidates": ["adb shell input tap: single tap only; multitouch unverified",
                                         "independent multi-contact backend: not selected"]}
    if serial is None:
        return report
    if serial not in [device["serial"] for device in devices if device["state"] == "device"]:
        raise ValueError("selected serial is not connected in device state")

    def shell(*arguments: str) -> str:
        return adb_call(adb, ["-s", serial, "shell", *arguments]).decode(errors="replace").strip()

    report["selected_serial"] = serial
    report["android_release"] = shell("getprop", "ro.build.version.release")
    report["android_sdk"] = shell("getprop", "ro.build.version.sdk")
    report["model"] = shell("getprop", "ro.product.model")
    report["cpu_abi"] = shell("getprop", "ro.product.cpu.abi")
    report["wm_size"] = shell("wm", "size")
    report["wm_density"] = shell("wm", "density")
    report["display_info"] = shell("dumpsys", "display")[:4000]
    return report


def decode_png_rgb(png: bytes) -> tuple[int, int, bytes]:
    """Decode non-interlaced 8-bit RGB/RGBA PNG emitted by screencap."""
    if not png.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("not a PNG screenshot")
    offset = 8
    compressed = bytearray()
    width = height = color_type = None
    while offset + 12 <= len(png):
        length = struct.unpack_from(">I", png, offset)[0]
        kind = png[offset + 4:offset + 8]
        data = png[offset + 8:offset + 8 + length]
        if len(data) != length or offset + 12 + length > len(png):
            raise ValueError("truncated PNG chunk")
        expected_crc = struct.unpack_from(">I", png, offset + 8 + length)[0]
        if binascii.crc32(kind + data) & 0xFFFFFFFF != expected_crc:
            raise ValueError("PNG CRC mismatch")
        offset += length + 12
        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", data)
            if (bit_depth, compression, filtering, interlace) != (8, 0, 0, 0) or color_type not in (2, 6):
                raise ValueError("unsupported PNG format")
            if width <= 0 or height <= 0 or width * height > 20_000_000:
                raise ValueError("unreasonable PNG dimensions")
        elif kind == b"IDAT":
            compressed.extend(data)
        elif kind == b"IEND":
            break
    if width is None or height is None:
        raise ValueError("missing PNG header")
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    raw = zlib.decompress(compressed)
    if len(raw) != (stride + 1) * height:
        raise ValueError("invalid PNG scanline length")
    rgb = bytearray(width * height * 3)
    previous = bytearray(stride)
    position = 0
    for y in range(height):
        filter_type = raw[position]
        position += 1
        line = bytearray(raw[position:position + stride])
        position += stride
        for i in range(stride):
            left = line[i - channels] if i >= channels else 0
            up = previous[i]
            upper_left = previous[i - channels] if i >= channels else 0
            if filter_type == 1:
                line[i] = (line[i] + left) & 255
            elif filter_type == 2:
                line[i] = (line[i] + up) & 255
            elif filter_type == 3:
                line[i] = (line[i] + (left + up) // 2) & 255
            elif filter_type == 4:
                prediction = left + up - upper_left
                distances = (abs(prediction - left), abs(prediction - up), abs(prediction - upper_left))
                choice = (left, up, upper_left)[distances.index(min(distances))]
                line[i] = (line[i] + choice) & 255
            elif filter_type != 0:
                raise ValueError("unsupported PNG filter")
        if channels == 3:
            rgb[y * width * 3:(y + 1) * width * 3] = line
        else:
            for x in range(width):
                src = x * 4
                dst = (y * width + x) * 3
                rgb[dst:dst + 3] = line[src:src + 3]
        previous = line
    return width, height, bytes(rgb)


class AdbPngCapture:
    def __init__(self, adb: str, serial: str, clock: Clock | None = None):
        self.adb, self.serial = adb, serial
        self.clock = clock or HostClock()
        self.sequence = 0

    def capture(self) -> tuple[Frame, int]:
        start_ns = self.clock.now_ns()
        png = adb_call(self.adb, ["-s", self.serial, "exec-out", "screencap", "-p"], 15)
        complete_ns = self.clock.now_ns()
        width, height, rgb = decode_png_rgb(png)
        frame = Frame(self.sequence, width, height, rgb, complete_ns)
        self.sequence += 1
        return frame, start_ns


class AdbLauncher:
    def __init__(self, adb: str, serial: str, clock: Clock | None = None):
        self.adb, self.serial = adb, serial
        self.clock = clock or HostClock()

    def launch(self, package: str) -> tuple[int, int]:
        if not package or not all(character.isalnum() or character in "._" for character in package):
            raise ValueError("invalid Android package name")
        installed = adb_call(self.adb, ["-s", self.serial, "shell", "pm", "path", package])
        if not installed.strip().startswith(b"package:"):
            raise RuntimeError(f"package {package} is not installed on selected device")
        start_ns = self.clock.now_ns()
        adb_call(self.adb, ["-s", self.serial, "shell", "monkey", "-p", package,
                            "-c", "android.intent.category.LAUNCHER", "1"], 15)
        return start_ns, self.clock.now_ns()
