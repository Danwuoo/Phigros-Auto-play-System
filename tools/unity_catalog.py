"""Read-only parser for the legacy JSON Unity Addressables catalog schema.

Independently implemented from Unity's package source, checked against 1.21.17:
https://github.com/needle-mirror/com.unity.addressables/blob/1.21.17/Runtime/ResourceLocators/ContentCatalogData.cs
https://github.com/needle-mirror/com.unity.addressables/blob/1.21.17/Runtime/Utility/SerializationUtilities.cs
https://docs.unity3d.com/Packages/com.unity.addressables@1.21/api/UnityEngine.ResourceManagement.ResourceLocations.IResourceLocation.html

Bucket byte offsets and typed keys are authoritative. A dependency key may
resolve to several locations; no bundle-to-single-asset overwrite map is made.
This module does not load Unity assemblies, bundles, devices, or remote URLs.
Only object tags 0..4 and JSON extra data (7) are supported; other tags fail.
"""
from base64 import b64decode
from binascii import Error as Base64Error
from dataclasses import dataclass
import json
from pathlib import Path
import re
import struct


class CatalogError(ValueError):
    """Unsupported, malformed, ambiguous, or cyclic catalog data."""


MAX_COUNT = 1000000
MAX_BLOB_BYTES = 64 * 1024 * 1024


class _Reader:
    def __init__(self, data, offset=0):
        self.data, self.pos = data, offset
        if not 0 <= offset <= len(data):
            raise CatalogError("Binary offset out of bounds")

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise CatalogError("Truncated binary catalog data")
        value = self.data[self.pos:self.pos + size]
        self.pos += size
        return value

    def integer(self, code="i"):
        return struct.unpack("<" + code, self.take(struct.calcsize("<" + code)))[0]

    def count(self):
        value = self.integer()
        if not 0 <= value <= MAX_COUNT:
            raise CatalogError("Invalid catalog count")
        return value

    def text(self, encoding, size=None):
        size = self.integer() if size is None else size
        try:
            return self.take(size).decode(encoding)
        except UnicodeError as exc:
            raise CatalogError("Invalid catalog string encoding") from exc


@dataclass(frozen=True)
class CatalogKey:
    kind: str
    value: str | int

    def to_dict(self):
        return dict(kind=self.kind, value=self.value)


@dataclass(frozen=True)
class JsonExtraData:
    assembly_name: str
    class_name: str
    value: object


def _object_at(blob, offset, *, key=False):
    reader = _Reader(blob, offset)
    tag = reader.integer("B")
    if tag in (0, 1):
        value = reader.text("ascii" if tag == 0 else "utf-16-le")
        return CatalogKey("string", value) if key else value
    if tag in (2, 3, 4):
        kind, code = {2: ("uint16", "H"), 3: ("uint32", "I"), 4: ("int32", "i")}[tag]
        value = reader.integer(code)
        return CatalogKey(kind, value) if key else value
    if tag == 7 and not key:
        assembly = reader.text("ascii", reader.integer("B"))
        name = reader.text("ascii", reader.integer("B"))
        try:
            value = json.loads(reader.text("utf-16-le"), parse_constant=_reject_constant)
        except (ValueError, TypeError) as exc:
            raise CatalogError("Invalid JSON extra data") from exc
        return JsonExtraData(assembly, name, value)
    raise CatalogError(f"Unsupported catalog object tag {tag}{' for a key' if key else ''}")


def _reject_constant(value):
    raise CatalogError("Nonfinite JSON constant is not supported")


def expand_internal_id(value, prefixes=()):
    """Apply Unity's last-# prefix rule; runtime path substitution is separate."""
    if not isinstance(value, str):
        raise CatalogError("Internal ID must be a string")
    if not prefixes:
        return value
    number, marker, suffix = value.rpartition("#")
    if not marker or not re.fullmatch(r"[+-]?[0-9]+", number.strip()):
        return value
    index = int(number)
    if not 0 <= index < len(prefixes):
        raise CatalogError("Internal ID prefix index out of bounds")
    if not isinstance(prefixes[index], str):
        raise CatalogError("Internal ID prefix must be a string")
    return prefixes[index] + suffix


@dataclass(frozen=True)
class CatalogEntry:
    index: int
    internal_id: str
    expanded_internal_id: str
    provider: str
    dependency_key_index: int | None
    dependency_hash: int
    primary_key: CatalogKey
    resource_type: object
    extra_data: object
    key_indices: tuple[int, ...]

    @property
    def primary_key_text(self):
        return str(self.primary_key.value)


def _blob(data, name):
    value = data.get(name)
    if not isinstance(value, str) or len(value) > (MAX_BLOB_BYTES * 4 // 3 + 4):
        raise CatalogError(f"Missing or oversized catalog field {name}")
    try:
        result = b64decode(value, validate=True)
    except (Base64Error, ValueError) as exc:
        raise CatalogError(f"Invalid base64 catalog field {name}") from exc
    if len(result) > MAX_BLOB_BYTES:
        raise CatalogError("Catalog binary field exceeds byte budget")
    return result


def _list(data, name, strings=False):
    value = data.get(name)
    if not isinstance(value, list) or len(value) > MAX_COUNT:
        raise CatalogError(f"Missing or invalid catalog array {name}")
    if strings and any(not isinstance(item, str) for item in value):
        raise CatalogError(f"Non-string value in catalog array {name}")
    return value


def _index(values, index, label):
    if not 0 <= index < len(values):
        raise CatalogError(f"{label} index out of bounds")
    return values[index]


class Catalog:
    @classmethod
    def from_bytes(cls, source):
        if len(source) > 128 * 1024 * 1024:
            raise CatalogError("Catalog JSON exceeds byte budget")
        try:
            data = json.loads(source, parse_constant=_reject_constant)
        except (UnicodeError, ValueError, TypeError) as exc:
            raise CatalogError("Invalid catalog JSON") from exc
        return cls.from_mapping(data)

    @classmethod
    def from_file(cls, path):
        path = Path(path)
        if path.stat().st_size > 128 * 1024 * 1024:
            raise CatalogError("Catalog JSON exceeds byte budget")
        return cls.from_bytes(path.read_bytes())

    @classmethod
    def from_mapping(cls, data):
        if not isinstance(data, dict):
            raise CatalogError("Catalog root must be an object")
        obj = cls()
        internal_ids = _list(data, "m_InternalIds", strings=True)
        providers = _list(data, "m_ProviderIds", strings=True)
        resource_types = _list(data, "m_resourceTypes")
        prefixes = data.get("m_InternalIdPrefixes")
        if prefixes is None:
            prefixes = []
        if not isinstance(prefixes, list) or any(not isinstance(value, str) for value in prefixes):
            raise CatalogError("Invalid internal ID prefixes")
        obj.locator_id = data.get("m_LocatorId")
        obj.build_result_hash = data.get("m_BuildResultHash")
        buckets = _Reader(_blob(data, "m_BucketDataString"))
        bucket_count = buckets.count()
        bucket_rows = []
        for _ in range(bucket_count):
            offset = buckets.integer()
            bucket_rows.append((offset, tuple(buckets.integer() for _ in range(buckets.count()))))
        if buckets.pos != len(buckets.data):
            raise CatalogError("Trailing bucket data is unsupported")
        key_blob = _blob(data, "m_KeyDataString")
        if _Reader(key_blob).count() != bucket_count:
            raise CatalogError("Key and bucket counts differ")
        keys = []
        for offset, _ in bucket_rows:
            if offset < 4:
                raise CatalogError("Key points into the key-count header")
            keys.append(_object_at(key_blob, offset, key=True))
        if len(set(keys)) != len(keys):
            raise CatalogError("Duplicate catalog keys are ambiguous")
        obj.keys = tuple(keys)
        obj._key_index = {key: i for i, key in enumerate(keys)}
        entry_blob = _Reader(_blob(data, "m_EntryDataString"))
        entry_count = entry_blob.count()
        if len(entry_blob.data) != 4 + entry_count * 28:
            raise CatalogError("Invalid entry table length")
        aliases = [[] for _ in range(entry_count)]
        obj._bucket_entries = tuple(row[1] for row in bucket_rows)
        for key_index, (_, indices) in enumerate(bucket_rows):
            if len(set(indices)) != len(indices):
                raise CatalogError("Repeated entry in one key bucket")
            for index in indices:
                _index(aliases, index, "Bucket entry").append(key_index)
        extra_blob = _blob(data, "m_ExtraDataString")
        entries = []
        for index in range(entry_count):
            iid, provider, dep, dep_hash, extra, primary, resource = (entry_blob.integer() for _ in range(7))
            raw_id = _index(internal_ids, iid, "Internal ID")
            if dep >= 0:
                _index(keys, dep, "Dependency key")
            entries.append(CatalogEntry(index, raw_id, expand_internal_id(raw_id, prefixes),
                _index(providers, provider, "Provider"), dep if dep >= 0 else None, dep_hash,
                _index(keys, primary, "Primary key"), _index(resource_types, resource, "Resource type"),
                _object_at(extra_blob, extra) if extra >= 0 else None, tuple(aliases[index])))
        obj.entries = tuple(entries)
        return obj

    def lookup(self, key):
        """Exact .NET-style typed-key lookup; substring matching is not identity."""
        if isinstance(key, str):
            key = CatalogKey("string", key)
        if not isinstance(key, CatalogKey):
            raise TypeError("Use a string or an explicitly typed CatalogKey")
        index = self._key_index.get(key)
        return () if index is None else tuple(self.entries[i] for i in self._bucket_entries[index])

    def _entry_index(self, entry):
        index = entry.index if isinstance(entry, CatalogEntry) else entry
        if isinstance(index, bool) or not isinstance(index, int):
            raise TypeError("Use a catalog entry or entry index")
        _index(self.entries, index, "Entry")
        if isinstance(entry, CatalogEntry) and entry is not self.entries[index]:
            raise CatalogError("Entry belongs to a different catalog")
        return index

    def dependencies(self, entry):
        entry = self.entries[self._entry_index(entry)]
        key_index = entry.dependency_key_index
        return () if key_index is None else tuple(self.entries[i] for i in self._bucket_entries[key_index])

    def dependency_closure(self, entry):
        """Dependencies first, excluding root; reject cycles without recursive calls."""
        root = self._entry_index(entry)
        state, result, stack = {}, [], [(root, False)]
        while stack:
            index, leaving = stack.pop()
            if leaving:
                state[index] = 2
                if index != root:
                    result.append(self.entries[index])
                continue
            if state.get(index) == 1:
                raise CatalogError("Cyclic catalog dependency graph")
            if state.get(index) == 2:
                continue
            state[index] = 1
            stack.append((index, True))
            stack.extend((dependency.index, False) for dependency in reversed(self.dependencies(index)))
        return tuple(result)


__all__ = ["Catalog", "CatalogEntry", "CatalogError", "CatalogKey", "JsonExtraData", "expand_internal_id"]
