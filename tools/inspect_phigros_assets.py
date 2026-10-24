"""Offline APK ZIP metadata inventory; no extraction, device access, or Unity loading."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import zipfile

MIB = 1024 * 1024
DEFAULT_LIMITS = dict(max_entry_bytes=1024 * MIB, max_total_bytes=8192 * MIB,
                      max_entries=100000, max_apk_bytes=4096 * MIB)


class InventoryError(ValueError):
    """Unsafe archive metadata or an explicit resource limit was encountered."""


def safe_entry_name(info):
    original = info.orig_filename
    name = original.replace("\\", "/")
    parts = name.rstrip("/").split("/")
    if (not name or name.startswith("/") or "\x00" in original or ":" in name
            or any(part in ("", ".", "..") for part in parts)
            or any(ord(char) < 32 for char in original)):
        raise InventoryError("Unsafe ZIP entry path")
    if info.filename != original:
        raise InventoryError("Ambiguous ZIP entry name")
    mode = info.external_attr >> 16
    if stat.S_ISLNK(mode) or stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR):
        raise InventoryError("ZIP symbolic links and special files are not supported")
    if info.flag_bits & (1 | 64 | 8192):
        raise InventoryError("Encrypted ZIP entries are not supported")
    if info.is_dir() and info.file_size:
        raise InventoryError("Directory entry declares resource bytes")
    return name


def inventory_apks(apk_paths, *, max_entry_bytes=DEFAULT_LIMITS["max_entry_bytes"],
                   max_total_bytes=DEFAULT_LIMITS["max_total_bytes"],
                   max_entries=DEFAULT_LIMITS["max_entries"],
                   max_apk_bytes=DEFAULT_LIMITS["max_apk_bytes"]):
    """Return a JSON-ready inventory for explicit paths; sizes/CRC are ZIP declarations.

    Budgets count declared uncompressed resource bytes and entries across all APKs.
    Entries are never extracted/decompressed. Each package SHA256 covers streamed bytes.
    Normalized duplicate/case-colliding names are refused for Windows-safe path identity.
    """
    limits = dict(max_entry_bytes=max_entry_bytes, max_total_bytes=max_total_bytes,
                  max_entries=max_entries, max_apk_bytes=max_apk_bytes)
    if any(not isinstance(value, int) or value <= 0 for value in limits.values()):
        raise ValueError("All inventory limits must be positive integers")
    paths = sorted((Path(path).resolve() for path in apk_paths), key=lambda path: str(path).casefold())
    if not paths:
        raise ValueError("Supply at least one explicit APK path")
    if len({str(path).casefold() for path in paths}) != len(paths):
        raise InventoryError("Duplicate APK input paths")
    packages, total_bytes, total_entries = [], 0, 0
    for path in paths:
        entries, seen = [], set()
        with path.open("rb") as handle:
            before = os.fstat(handle.fileno())
            if before.st_size > max_apk_bytes:
                raise InventoryError("APK file byte budget exceeded")
            digest = hashlib.sha256()
            for chunk in iter(lambda: handle.read(MIB), b""):
                digest.update(chunk)
            handle.seek(0)
            try:
                with zipfile.ZipFile(handle) as archive:
                    infos = archive.infolist()
                    if total_entries + len(infos) > max_entries:
                        raise InventoryError("ZIP entry count budget exceeded")
                    for info in infos:
                        name = safe_entry_name(info)
                        identity = name.rstrip("/").casefold()
                        if identity in seen:
                            raise InventoryError("Duplicate or case-colliding ZIP entry name")
                        seen.add(identity)
                        if info.file_size > max_entry_bytes:
                            raise InventoryError("Single ZIP entry resource budget exceeded")
                        if info.compress_size > before.st_size:
                            raise InventoryError("ZIP entry compressed size exceeds APK size")
                        total_bytes += info.file_size
                        total_entries += 1
                        if total_bytes > max_total_bytes:
                            raise InventoryError("Total ZIP resource byte budget exceeded")
                        entries.append(dict(path=name, type="directory" if info.is_dir() else "file",
                                            bytes=info.file_size, compressed_bytes=info.compress_size,
                                            crc32=f"{info.CRC:08x}", compression_method=info.compress_type))
            except zipfile.BadZipFile as error:
                raise InventoryError("Invalid APK ZIP structure") from error
            handle_stat = os.fstat(handle.fileno())
            if (before.st_size, before.st_mtime_ns, before.st_ino) != (handle_stat.st_size, handle_stat.st_mtime_ns, handle_stat.st_ino):
                raise InventoryError("APK changed during inventory")
        entries.sort(key=lambda entry: entry["path"])
        files = [entry["path"] for entry in entries if entry["type"] == "file"]
        packages.append(dict(path=str(path), name=path.name, bytes=before.st_size,
                             sha256=digest.hexdigest(), entries=entries,
                             catalog_candidates=[name for name in files if "catalog" in name.rsplit("/", 1)[-1].casefold()],
                             dlyrotz_name_candidates=[name for name in files if "dlyrotz" in name.casefold()]))
    return dict(schema_version=1, inspection="offline_apk_zip_metadata", limits=limits,
                packages=packages, total_entries=total_entries, total_resource_bytes=total_bytes,
                validation="Package SHA256 covers bytes. ZIP sizes/CRC are declarations; entry content CRC is not verified.",
                candidate_scope="Case-insensitive path-name matches only; no Unity parsing or asset identity claims.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apks", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, help="New JSON file; existing paths are refused. Default: stdout.")
    parser.add_argument("--max-entry-mib", type=int, default=1024)
    parser.add_argument("--max-total-mib", type=int, default=8192)
    parser.add_argument("--max-apk-mib", type=int, default=4096)
    parser.add_argument("--max-entries", type=int, default=100000)
    args = parser.parse_args(argv)
    if args.output is not None and args.output.exists():
        raise FileExistsError("Inventory output already exists")
    inventory = inventory_apks(args.apks, max_entry_bytes=args.max_entry_mib * MIB,
                               max_total_bytes=args.max_total_mib * MIB,
                               max_apk_bytes=args.max_apk_mib * MIB, max_entries=args.max_entries)
    if args.output is None:
        print(json.dumps(inventory, ensure_ascii=False, indent=2))
    else:
        with args.output.open("x", encoding="utf-8") as handle:
            json.dump(inventory, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
