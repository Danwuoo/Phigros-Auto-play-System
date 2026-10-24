"""Synthetic APK fixtures only; no downloads, ADB, or real game assets."""
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import tempfile
import unittest
import warnings
import zipfile
import zlib

SPEC = importlib.util.spec_from_file_location(
    "inspect_phigros_assets", Path(__file__).parents[1] / "tools/inspect_phigros_assets.py")
inventory = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inventory)


class PhigrosAssetInventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def apk(self, name="fixture.apk", entries=None):
        path = self.root / name
        with warnings.catch_warnings(), zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
            warnings.simplefilter("ignore", UserWarning)
            for entry, content in entries or [("assets/aa/catalog.json", b"synthetic catalog"),
                                             ("assets/music/Dlyrotz.fake", b"synthetic song")]:
                archive.writestr(entry, content)
        return path

    def test_metadata_hash_crc_candidates_and_reproducibility(self):
        path = self.apk(entries=[("z.txt", b"z"), ("assets/DLYROTZ.fake", b"song"),
                                 ("assets/aa/catalog.json", b"catalog")])
        result = inventory.inventory_apks([path])
        package = result["packages"][0]
        self.assertEqual(package["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(package["bytes"], path.stat().st_size)
        self.assertEqual(package["catalog_candidates"], ["assets/aa/catalog.json"])
        self.assertEqual(package["dlyrotz_name_candidates"], ["assets/DLYROTZ.fake"])
        self.assertEqual([row["path"] for row in package["entries"]], sorted(row["path"] for row in package["entries"]))
        song = next(row for row in package["entries"] if row["path"].endswith("fake"))
        self.assertEqual((song["bytes"], song["crc32"]), (4, f"{zlib.crc32(b'song'):08x}"))
        self.assertEqual(result, inventory.inventory_apks([path]))

    def test_duplicate_and_case_colliding_names_are_rejected(self):
        for index, names in enumerate([("same", "same"), ("Assets/x", "assets/X"),
                                       ("assets/x", "assets\\x"), ("assets/", "assets")]):
            with self.subTest(names=names):
                path = self.apk(f"duplicate-{index}.apk", [(name, b"") for name in names])
                with self.assertRaises(inventory.InventoryError):
                    inventory.inventory_apks([path])

    def test_absolute_traversal_drive_and_ambiguous_paths_are_rejected(self):
        for index, name in enumerate(["../escape", "assets/../escape", "/absolute", "C:/drive",
                                       "assets/./ambiguous", "assets//double", "..\\escape"]):
            with self.subTest(name=name):
                path = self.apk(f"unsafe-{index}.apk", [(name, b"x")])
                with self.assertRaises(inventory.InventoryError):
                    inventory.inventory_apks([path])

    def test_encrypted_entry_is_rejected_from_central_directory_flag(self):
        path = self.apk(entries=[("file", b"x")])
        data = bytearray(path.read_bytes())
        offset = data.index(b"PK\x01\x02") + 8
        flags = int.from_bytes(data[offset:offset + 2], "little") | 1
        data[offset:offset + 2] = flags.to_bytes(2, "little")
        path.write_bytes(data)
        with self.assertRaisesRegex(inventory.InventoryError, "Encrypted"):
            inventory.inventory_apks([path])

    def test_symlink_is_rejected(self):
        info = zipfile.ZipInfo("assets/link")
        info.create_system, info.external_attr = 3, (stat.S_IFLNK | 0o777) << 16
        path = self.apk(entries=[(info, b"../target")])
        with self.assertRaisesRegex(inventory.InventoryError, "symbolic"):
            inventory.inventory_apks([path])

    def test_entry_total_archive_and_count_budgets(self):
        first = self.apk("one.apk", [("one", b"1" * 20)])
        second = self.apk("two.apk", [("two", b"2" * 20)])
        cases = [([first], dict(max_entry_bytes=19)), ([first, second], dict(max_total_bytes=39)),
                 ([first], dict(max_apk_bytes=first.stat().st_size - 1)),
                 ([first, second], dict(max_entries=1))]
        for paths, limits in cases:
            with self.subTest(limits=limits), self.assertRaises(inventory.InventoryError):
                inventory.inventory_apks(paths, **limits)

    def test_inventory_does_not_open_or_decompress_entries(self):
        from unittest import mock
        path = self.apk()
        with mock.patch.object(zipfile.ZipFile, "open", side_effect=AssertionError("entry opened")):
            self.assertEqual(inventory.inventory_apks([path])["total_entries"], 2)

    def test_output_refuses_overwrite_and_writes_valid_json(self):
        path, output = self.apk(), self.root / "inventory.json"
        inventory.main([str(path), "--output", str(output)])
        original = output.read_bytes()
        self.assertEqual(len(json.loads(original)["packages"]), 1)
        with self.assertRaises(FileExistsError):
            inventory.main([str(path), "--output", str(output)])
        self.assertEqual(output.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
