import copy
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))
import song_catalog as catalog
import sync_song_catalog as sync


class Response(io.BytesIO):
    def __init__(self, raw, status=200, **headers):
        super().__init__(raw)
        self.status = status
        self.headers = {"Content-Length": str(len(raw)), **headers}


def native_chart(version=3, notes=1):
    return catalog.json_bytes(dict(formatVersion=version, offset=0.25, judgeLineList=[dict(
        bpm=120, notesAbove=[dict(type=4, time=64 + i, holdTime=0) for i in range(notes)], notesBelow=[])]))


class CatalogFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "本機 曲庫"
        self.root.mkdir()
        self.config = dict(schema=catalog.SOURCE_SCHEMA, repository="Example/Resource", version="4.0.1.2",
                           commits=dict(info="1" * 40, chart="2" * 40, music="3" * 40),
                           expected=dict(song_count=2, IN=3, AT=1, variant_count=1, audio_count=2),
                           native_runtime_revision="4.0.1/157")
        self.raw = {
            "info": {"info.tsv": "歌曲.Composer\t歌曲\tComposer\tArtist\tEZ\tHD\tIN\tAT\textra\nRandom.Pair\tRandom\tPair\tArtist\tEZ\tHD\tIN\n".encode(),
                     "difficulty.tsv": b"\xef\xbb\xbf" + "歌曲.Composer\t3.0\t8.0\t14.5\t16.0\nRandom.Pair\t3.0\t8.0\t14.0\n".encode(),
                     "version.txt": b"4.0.1.2\n"},
            "chart": {"歌曲.Composer.0/IN.json": native_chart(), "歌曲.Composer.0/AT.json": native_chart(notes=2),
                      "Random.Pair.0/IN.json": native_chart(1), "Random.Pair.1/IN.json": native_chart()},
            "music": {"歌曲.Composer.ogg": b"synthetic OGG fixture one", "Random.Pair.ogg": b"synthetic OGG fixture two"}}
        self.trees = {branch: self.tree(files) for branch, files in self.raw.items()}
        self.lock = self.make_lock()

    @staticmethod
    def tree(files):
        return dict(sha="4" * 40, truncated=False, tree=[dict(path=path, type="blob", mode="100644",
                    sha=catalog.git_blob_sha(raw), size=len(raw)) for path, raw in files.items()])

    def make_lock(self):
        return catalog.build_lock(self.config, self.trees["info"], self.trees["chart"], self.trees["music"],
                                  self.raw["info"]["info.tsv"], self.raw["info"]["difficulty.tsv"], self.raw["info"]["version.txt"])

    def populate(self):
        for item in self.lock["targets"]:
            catalog.write_atomic(self.root, item["file"], self.raw[item["branch"]][item["source_path"]])

    def target(self):
        return next(t for t in self.lock["targets"] if t["kind"] == "chart")


class MetadataTests(CatalogFixture):
    def test_variants_and_ragged_metadata_preserved(self):
        self.assertEqual(self.lock["counts"], self.config["expected"])
        first = self.lock["songs"][0]
        self.assertEqual(first["title"], "歌曲")
        self.assertEqual(first["extra_metadata"], ["extra"])
        self.assertNotIn("AT", self.lock["songs"][1]["ratings"])
        targets = [t for t in self.lock["targets"] if t["kind"] == "chart"]
        self.assertEqual(len(targets), 4)
        variant = next(x for x in targets if x["variant"] == "1")
        self.assertEqual(variant["song_id"], "Random.Pair")
        self.assertEqual(variant["track_id"], "Random.Pair.1")

    def test_metadata_does_not_invent_at_or_chapters(self):
        for song in self.lock["songs"]:
            self.assertIsNone(song["chapter"])
        self.assertEqual(sum("AT" in s["ratings"] for s in self.lock["songs"]), 1)

    def test_floating_refs_rejected(self):
        self.config["commits"]["chart"] = "main"
        with self.assertRaisesRegex(ValueError, "full commit"):
            self.make_lock()

    def test_truncated_tree_rejected(self):
        self.trees["chart"]["truncated"] = True
        with self.assertRaisesRegex(ValueError, "truncated"):
            self.make_lock()

    def test_metadata_tree_fingerprint_checked(self):
        self.raw["info"]["info.tsv"] += b"\n"
        with self.assertRaisesRegex(ValueError, "pinned tree"):
            self.make_lock()

    def test_duplicate_song_metadata_rejected(self):
        raw = self.raw["info"]["info.tsv"]
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            catalog.parse_metadata(raw + raw, self.raw["info"]["difficulty.tsv"])

    def test_missing_declared_at_rejected(self):
        self.trees["chart"]["tree"] = [x for x in self.trees["chart"]["tree"] if not x["path"].endswith("AT.json")]
        with self.assertRaisesRegex(ValueError, "missing basic chart"):
            self.make_lock()

    def test_version_and_inventory_disagreement_rejected(self):
        self.config["expected"]["IN"] = 99
        with self.assertRaisesRegex(ValueError, "inventory differs"):
            self.make_lock()
        self.config["expected"]["IN"] = 3
        self.raw["info"]["version.txt"] = b"4.0.2\n"
        with self.assertRaisesRegex(ValueError, "version differs"):
            self.make_lock()

    def test_destination_traversal_and_case_collision_rejected(self):
        for bad in ("../file", "/tmp/file", "C:/file", "foo\\bar", "foo/../bar", "aux.json", "file."):
            with self.subTest(path=bad), self.assertRaises(ValueError):
                catalog.safe_relative(bad)
        lock = copy.deepcopy(self.lock)
        lock["targets"].append(copy.deepcopy(lock["targets"][0]))
        with self.assertRaisesRegex(ValueError, "case-colliding"):
            catalog.validate_lock(lock)

    def test_lock_rejects_rewritten_host_and_source_path(self):
        lock = copy.deepcopy(self.lock)
        lock["targets"][0]["raw_url"] = "https://example.invalid/secret"
        with self.assertRaisesRegex(ValueError, "URL differs"):
            catalog.validate_lock(lock)


class IntegrityTests(CatalogFixture):
    def test_git_blob_identity_includes_header(self):
        self.assertEqual(catalog.git_blob_sha(b"hello\n"), "ce013625030ba8dba906f756967f9e9ca394464a")

    def test_missing_corrupt_and_pointer_distinct(self):
        target = self.target()
        self.assertEqual(catalog.inspect_asset(self.root, target)["status"], "missing")
        catalog.write_atomic(self.root, target["file"], b"wrong")
        self.assertEqual(catalog.inspect_asset(self.root, target)["error"], "size_mismatch")
        pointer = catalog.LFS_HEADER + b"oid sha256:" + b"a" * 64 + b"\nsize 100\n"
        catalog.write_atomic(self.root, target["file"], pointer)
        self.assertEqual(catalog.inspect_asset(self.root, target)["status"], "lfs_pointer")

    def test_equal_size_wrong_bytes_fail_blob_check(self):
        target = self.target()
        catalog.write_atomic(self.root, target["file"], b"x" * target["bytes"])
        observed = catalog.inspect_asset(self.root, target)
        self.assertEqual(observed["error"], "git_blob_mismatch")
        self.assertEqual(len(observed["sha256"]), 64)

    def test_symlink_traversal_rejected(self):
        external = Path(self.temporary.name) / "external"
        external.mkdir()
        try:
            (self.root / "charts").symlink_to(external, target_is_directory=True)
        except OSError:
            self.skipTest("Symlink privileges unavailable")
        with self.assertRaisesRegex(ValueError, "symlinks"):
            catalog.inspect_asset(self.root, self.target())

    def test_immutable_snapshot_refuses_overwrite(self):
        catalog.write_atomic(self.root, "lock.json", b"old", immutable=True)
        catalog.write_atomic(self.root, "lock.json", b"old", immutable=True)
        with self.assertRaisesRegex(ValueError, "immutable snapshot differs"):
            catalog.write_atomic(self.root, "lock.json", b"new", immutable=True)
        self.assertEqual((self.root / "lock.json").read_bytes(), b"old")


class DownloadTests(CatalogFixture):
    def opener(self, *, range_support=True, bad=False):
        target = self.target()
        data = self.raw[target["branch"]][target["source_path"]]
        calls = []

        def open_fake(url, start=0):
            calls.append(start)
            raw = b"x" * len(data) if bad else data
            if start and range_support:
                return Response(raw[start:], status=206, **{"Content-Range": f"bytes {start}-{len(raw)-1}/{len(raw)}"})
            return Response(raw)
        return open_fake, calls, data

    def test_download_then_reuse_is_byte_exact(self):
        opener, calls, data = self.opener()
        target = self.target()
        result = sync.download_asset(self.root, target, opener=opener)
        self.assertEqual(result["status"], "verified")
        self.assertEqual(result["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual((self.root / target["file"]).read_bytes(), data)
        result = sync.download_asset(self.root, target, opener=opener)
        self.assertEqual(result["action"], "reused")
        self.assertEqual(calls, [0])

    def test_partial_resume_and_server_ignored_range(self):
        for honored in (True, False):
            with self.subTest(range_supported=honored):
                opener, calls, data = self.opener(range_support=honored)
                target = self.target()
                path = self.root / target["file"]
                path.unlink(missing_ok=True)
                catalog.write_atomic(self.root, target["file"] + ".part", data[:10])
                result = sync.download_asset(self.root, target, opener=opener)
                self.assertEqual(result["status"], "verified")
                self.assertEqual(calls, [10])
                self.assertEqual(path.read_bytes(), data)

    def test_bad_download_never_promoted(self):
        opener, calls, _ = self.opener(bad=True)
        target = self.target()
        result = sync.download_asset(self.root, target, retries=2, opener=opener, sleep=lambda n: None)
        self.assertEqual(result["status"], "failed")
        self.assertEqual(len(calls), 2)
        self.assertFalse((self.root / target["file"]).exists())

    def test_existing_corrupt_file_requires_explicit_repair(self):
        target = self.target()
        opener, calls, data = self.opener()
        catalog.write_atomic(self.root, target["file"], b"corrupt")
        result = sync.download_asset(self.root, target, opener=opener)
        self.assertEqual(result["action"], "refused")
        self.assertEqual(calls, [])
        result = sync.download_asset(self.root, target, repair=True, opener=opener)
        self.assertEqual(result["status"], "verified")
        self.assertEqual((self.root / target["file"]).read_bytes(), data)
        backups = list((self.root / target["file"]).parent.glob("*.corrupt-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b"corrupt")

    def test_lfs_pointer_can_be_hydrated(self):
        target = self.target()
        opener, calls, data = self.opener()
        catalog.write_atomic(self.root, target["file"], catalog.LFS_HEADER + b"oid sha256:" + b"f" * 64 + b"\nsize 123\n")
        self.assertEqual(sync.download_asset(self.root, target, opener=opener)["status"], "verified")
        self.assertEqual((self.root / target["file"]).read_bytes(), data)

    def test_complete_partial_verified_before_promotion(self):
        target = self.target()
        opener, calls, data = self.opener()
        catalog.write_atomic(self.root, target["file"] + ".part", data)
        self.assertEqual(sync.download_asset(self.root, target, opener=opener)["status"], "verified")
        self.assertEqual(calls, [])

    def test_invalid_content_range_rejected(self):
        target = self.target()
        _, _, data = self.opener()
        catalog.write_atomic(self.root, target["file"] + ".part", data[:10])
        result = sync.download_asset(self.root, target, retries=1,
            opener=lambda url, start: Response(data[10:], status=206, **{"Content-Range": "bytes 0-10/20"}))
        self.assertEqual(result["status"], "failed")
        self.assertFalse((self.root / target["file"]).exists())


class LibraryTests(CatalogFixture):
    def test_all_targets_visible_when_files_missing(self):
        library = catalog.build_library(self.root, self.lock)
        self.assertEqual(library["schema"], "pas.catalog-library.v1")
        self.assertEqual(len(library["charts"]), 4)
        self.assertEqual(library["counts"]["verified_chart_count"], 0)
        self.assertTrue(all(x["archive_status"] == "missing" for x in library["charts"]))
        self.assertFalse(any(x["live_ready"] for x in library["charts"]))

    def test_native_bytes_preserved_and_never_marked_live_ready(self):
        self.populate()
        library = catalog.build_library(self.root, self.lock)
        self.assertEqual(library["status"], "complete")
        self.assertEqual(library["counts"]["verified_chart_count"], 4)
        self.assertEqual(library["counts"]["verified_audio_count"], 2)
        self.assertEqual(library["counts"]["unsupported_format_count"], 1)
        for row in library["charts"]:
            self.assertFalse(row["live_ready"])
            self.assertFalse(row["source"]["game_version_exact"])
            self.assertEqual(row["prepare_status"], "blocked")
            self.assertIn("installed_version_unverified", {x["code"] for x in row["blockers"]})
            self.assertIsNone(row["legacy_library_id"])
        at = next(row for row in library["charts"] if row["difficulty"] == "AT")
        self.assertEqual(at["notes"], 2)
        self.assertEqual(at["summary"]["first_note_seconds"], 1.25)
        self.assertIn("at_hud_required", {x["code"] for x in at["blockers"]})
        variant = next(row for row in library["charts"] if row["variant"] == "1")
        self.assertIn("variant_binding_required", {x["code"] for x in variant["blockers"]})

    def test_deterministic_index(self):
        self.populate()
        a = catalog.json_bytes(catalog.build_library(self.root, self.lock))
        b = catalog.json_bytes(catalog.build_library(self.root, self.lock))
        self.assertEqual(a, b)
        observed = sync.verify_catalog(self.root, self.lock)
        for value in observed.values():
            value.update(action="downloaded", attempts=2)
        c = catalog.json_bytes(catalog.build_library(self.root, self.lock, observed))
        self.assertEqual(a, c)

    def test_note_time_and_json_nonfinite_rejected(self):
        path = self.root / "bad.json"
        path.write_bytes(b'{"formatVersion":3,"offset":NaN,"judgeLineList":[]}')
        with self.assertRaisesRegex(ValueError, "Non-finite"):
            catalog.chart_summary(path)

    def test_verify_is_offline_and_detects_changed_file(self):
        self.populate()
        before = sync.verify_catalog(self.root, self.lock)
        self.assertTrue(all(x["status"] == "verified" for x in before.values()))
        target = self.target()
        (self.root / target["file"]).write_bytes(b"changed")
        after = sync.verify_catalog(self.root, self.lock)
        self.assertEqual(after[target["file"]]["status"], "failed")

    def test_chart_only_keeps_audio_missing_visible(self):
        for target in self.lock["targets"]:
            if target["kind"] != "audio":
                catalog.write_atomic(self.root, target["file"], self.raw[target["branch"]][target["source_path"]])
        results = sync.verify_catalog(self.root, self.lock, charts_only=True)
        library, report = sync.save_outputs(self.root, self.lock, results, "charts-only")
        self.assertEqual(report["failed"], 0)
        self.assertEqual(library["status"], "incomplete")
        self.assertEqual(library["counts"]["verified_audio_count"], 0)
        self.assertTrue(all("audio_unavailable" in {b["code"] for b in x["blockers"]} for x in library["charts"]))


if __name__ == "__main__":
    unittest.main()
