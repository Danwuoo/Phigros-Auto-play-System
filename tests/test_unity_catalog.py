"""Synthetic offline catalog contracts; no device, UnityPy, or private assets."""
from base64 import b64encode
import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest

SPEC = importlib.util.spec_from_file_location("unity_catalog", Path(__file__).parents[1] / "tools/unity_catalog.py")
module = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = module
SPEC.loader.exec_module(module)


def integer(value):
    return struct.pack("<i", value)


def string(value, unicode=False):
    data = value.encode("utf-16-le" if unicode else "ascii")
    return bytes([int(unicode)]) + integer(len(data)) + data


def catalog(keys=None, buckets=None, entries=None, physical_order=None, extra=b""):
    keys = keys or [string("Assets/Tracks/Example/Chart_IN.json"), string("Assets/Tracks/Example/music.ogg"), string("bundle-A"), string("bundle-B"), string("both")]
    buckets = buckets or [[0], [1], [2], [3], [2, 3]]
    entries = entries or [(0, 0, 4, 0, -1, 0, 0), (1, 0, 4, 0, -1, 1, 0),
                          (2, 1, -1, 0, -1, 2, 1), (3, 1, -1, 0, -1, 3, 1)]
    physical_order = list(range(len(keys))) if physical_order is None else physical_order
    blob, offsets = bytearray(integer(len(keys))), {}
    for index in physical_order:
        offsets[index] = len(blob)
        blob.extend(keys[index])
    bucket_blob = bytearray(integer(len(buckets)))
    for index, indices in enumerate(buckets):
        bucket_blob.extend(integer(offsets[index]) + integer(len(indices)) + b"".join(integer(i) for i in indices))
    encode = lambda value: b64encode(value).decode("ascii")
    return dict(m_InternalIds=["Assets/Tracks/Example/Chart_IN.json", "Assets/Tracks/Example/music.ogg", "0#bundle-A.bundle", "0#bundle-B.bundle"],
        m_InternalIdPrefixes=["{RuntimePath}/Android/"], m_ProviderIds=["AssetProvider", "AssetBundleProvider"],
        m_resourceTypes=[dict(m_ClassName="TextAsset"), dict(m_ClassName="AssetBundle")],
        m_KeyDataString=encode(blob), m_BucketDataString=encode(bucket_blob),
        m_EntryDataString=encode(integer(len(entries)) + b"".join(struct.pack("<7i", *entry) for entry in entries)),
        m_ExtraDataString=encode(extra))


class CatalogTests(unittest.TestCase):
    def test_shared_bundle_dependencies_do_not_overwrite_assets(self):
        parsed = module.Catalog.from_mapping(catalog())
        chart = parsed.lookup("Assets/Tracks/Example/Chart_IN.json")[0]
        music = parsed.lookup("Assets/Tracks/Example/music.ogg")[0]
        self.assertNotEqual(chart.index, music.index)
        self.assertEqual([e.index for e in parsed.dependencies(chart)], [2, 3])
        self.assertEqual([e.index for e in parsed.dependencies(music)], [2, 3])
        self.assertEqual(parsed.lookup("bundle-A")[0].expanded_internal_id, "{RuntimePath}/Android/bundle-A.bundle")

    def test_keys_follow_bucket_offsets_not_physical_object_order(self):
        parsed = module.Catalog.from_mapping(catalog(physical_order=[4, 2, 0, 3, 1]))
        self.assertEqual(parsed.lookup("Assets/Tracks/Example/Chart_IN.json")[0].index, 0)
        self.assertEqual([e.index for e in parsed.lookup("both")], [2, 3])

    def test_lookup_is_exact_and_case_sensitive(self):
        parsed = module.Catalog.from_mapping(catalog())
        self.assertEqual(parsed.lookup("Chart_IN.json"), ())
        self.assertEqual(parsed.lookup("assets/tracks/example/chart_in.json"), ())

    def test_numeric_key_types_are_distinct(self):
        keys = [bytes([2]) + struct.pack("<H", 7), bytes([3]) + struct.pack("<I", 7), bytes([4]) + integer(7)]
        parsed = module.Catalog.from_mapping(catalog(keys=keys, buckets=[[0], [1], [2]],
            entries=[(0, 0, -1, 0, -1, 0, 0), (1, 0, -1, 0, -1, 1, 0), (2, 1, -1, 0, -1, 2, 1)]))
        for index, kind in enumerate(("uint16", "uint32", "int32")):
            self.assertEqual(parsed.lookup(module.CatalogKey(kind, 7))[0].index, index)
        with self.assertRaises(TypeError):
            parsed.lookup(7)

    def test_unicode_key_and_json_extra_are_decoded_without_loading_assemblies(self):
        assembly, name, value = b"Example", b"Options", {"crc": 42, "label": "測試"}
        payload = json.dumps(value, ensure_ascii=False).encode("utf-16-le")
        extra = bytes([7, len(assembly)]) + assembly + bytes([len(name)]) + name + integer(len(payload)) + payload
        data = catalog(keys=[string("測試", unicode=True)], buckets=[[0]], entries=[(0, 0, -1, 0, 0, 0, 0)], extra=extra)
        parsed = module.Catalog.from_mapping(data)
        entry = parsed.lookup("測試")[0]
        self.assertEqual(entry.extra_data.value, value)
        self.assertEqual(entry.extra_data.assembly_name, "Example")

    def test_dependency_closure_is_unique_dependencies_first_and_detects_cycles(self):
        entries = [(0, 0, 4, 0, -1, 0, 0), (1, 0, -1, 0, -1, 1, 0),
                   (2, 1, 3, 0, -1, 2, 1), (3, 1, -1, 0, -1, 3, 1)]
        parsed = module.Catalog.from_mapping(catalog(entries=entries))
        self.assertEqual([e.index for e in parsed.dependency_closure(0)], [3, 2])
        entries[3] = (3, 1, 0, 0, -1, 3, 1)
        with self.assertRaises(module.CatalogError):
            module.Catalog.from_mapping(catalog(entries=entries)).dependency_closure(0)

    def test_internal_id_prefix_rule_and_invalid_prefix(self):
        self.assertEqual(module.expand_internal_id("0#file", ["root/"]), "root/file")
        self.assertEqual(module.expand_internal_id("folder#file", ["root/"]), "folder#file")
        self.assertEqual(module.expand_internal_id("0#file", []), "0#file")
        with self.assertRaises(module.CatalogError):
            module.expand_internal_id("4#file", ["root/"])
        with self.assertRaises(module.CatalogError):
            module.expand_internal_id("-1#file", ["root/"])
        data = catalog()
        data["m_InternalIdPrefixes"] = False
        with self.assertRaises(module.CatalogError):
            module.Catalog.from_mapping(data)

    def test_rejects_unknown_key_tags_and_out_of_bounds_indices(self):
        with self.assertRaises(module.CatalogError):
            module.Catalog.from_mapping(catalog(keys=[b"\x05"], buckets=[[0]], entries=[(0, 0, -1, 0, -1, 0, 0)]))
        for item in ((99, 0, -1, 0, -1, 0, 0), (0, 99, -1, 0, -1, 0, 0), (0, 0, 99, 0, -1, 0, 0), (0, 0, -1, 0, -1, 99, 0), (0, 0, -1, 0, -1, 0, 99)):
            with self.subTest(item=item), self.assertRaises(module.CatalogError):
                module.Catalog.from_mapping(catalog(keys=[string("key")], buckets=[[0]], entries=[item]))

    def test_rejects_truncation_bad_base64_and_duplicate_keys(self):
        for field in ("m_BucketDataString", "m_KeyDataString", "m_EntryDataString"):
            data = catalog()
            data[field] = b64encode(b"\x01").decode()
            with self.subTest(field=field), self.assertRaises(module.CatalogError):
                module.Catalog.from_mapping(data)
        data = catalog()
        data["m_KeyDataString"] = "!notbase64!"
        with self.assertRaises(module.CatalogError):
            module.Catalog.from_mapping(data)
        with self.assertRaises(module.CatalogError):
            module.Catalog.from_mapping(catalog(keys=[string("same"), string("same")], buckets=[[0], [0]], entries=[(0, 0, -1, 0, -1, 0, 0)]))

    def test_json_bytes_and_foreign_entries_are_rejected(self):
        first = module.Catalog.from_bytes(json.dumps(catalog()).encode())
        second = module.Catalog.from_mapping(catalog())
        with self.assertRaises(module.CatalogError):
            first.dependencies(second.entries[0])
        with self.assertRaises(module.CatalogError):
            module.Catalog.from_bytes(b'{"value":NaN}')


if __name__ == "__main__":
    unittest.main()
