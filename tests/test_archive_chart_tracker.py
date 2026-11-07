"""Offline archive boundaries, byte provenance, and unsupported-field tests."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import archive_chart_tracker as archive


def chart(**extra):
    value = dict(formatVersion=3, judgeLineList=[dict(notesAbove=[dict(type=1)], notesBelow=[])])
    value.update(extra)
    return value


class BlockAreaTests(unittest.TestCase):
    def test_recursive_spelling_states_and_pointer_escaping(self):
        value = {"a/b": [{"blockAreaList": []}, {"block area list": [1]},
                         {"BLOCK_AREA_LIST": None}], "x": {"block-area-list": {}}}
        fields = archive.block_area_fields(value)
        self.assertEqual([x["path"] for x in fields], ["/a~1b/0/blockAreaList", "/a~1b/1/block area list",
                                                       "/a~1b/2/BLOCK_AREA_LIST", "/x/block-area-list"])
        self.assertEqual([x["state"] for x in fields], ["empty", "nonempty", "invalid", "invalid"])
        self.assertEqual([x["entries"] for x in fields], [0, 1, None, None])

    def test_empty_nonempty_and_invalid_are_all_explicitly_unsupported(self):
        for field in ([], [dict(x=1)], None, {}, ""):
            with self.subTest(field=field):
                summary, status, blockers = archive.analyze_chart(archive.json_bytes(chart(blockAreaList=field)))
                self.assertEqual(status, "unsupported_block_area")
                self.assertEqual(summary["block_area_status"], "present")
                self.assertEqual(blockers[0]["code"], "unsupported_block_area")
                self.assertIn("目前無法完成", blockers[0]["message"])

    def test_absent_and_unknown_format_are_distinct(self):
        summary, status, _ = archive.analyze_chart(archive.json_bytes(chart()))
        self.assertEqual((summary["block_area_status"], status), ("absent", "structural_only"))
        summary, status, blockers = archive.analyze_chart(archive.json_bytes(chart(formatVersion=4)))
        self.assertEqual(status, "unsupported_format")
        self.assertEqual(blockers[0]["code"], "unsupported_format")
        summary, status, _ = archive.analyze_chart(b"not JSON")
        self.assertEqual((summary["block_area_status"], status), ("unknown", "failed"))


@unittest.skipUnless(shutil.which("git"), "Git is required for offline blob fixtures")
class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.output = self.root / "archive"
        self.source.mkdir()
        self.run_git("init", "-q")
        # This fixture prevents a global autocrlf setting from mutating the
        # exact bytes committed for the expected source identity.
        self.run_git("config", "core.autocrlf", "false")
        self.raw = b"\xef\xbb\xbf" + archive.json_bytes(chart(blockAreaList=[]))
        self.put("charts/Exact.Artist.0/Chart_IN.json", self.raw)
        self.put("charts/Exact.Artist.0/Chart_AT.json", archive.json_bytes(chart(formatVersion=4)))
        self.put("charts/Exact.Artist.0/Chart_IN_Error.json", b"upstream extractor error: unchanged\r\n")
        self.put("charts/Exact.Artist.0/Chart_EZ.json", archive.json_bytes(chart()))
        self.put("version.json", archive.json_bytes(dict(version="4.0.1.2", versioncode="160")))
        self.put("README.md", b"Upstream provenance and retention statement.\n")
        self.commit()

    def tearDown(self):
        self.temporary.cleanup()

    def run_git(self, *args):
        return subprocess.run(["git", "-C", str(self.source), *args], check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout

    def put(self, relative, raw):
        path = self.source / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)

    def commit(self):
        self.run_git("add", ".")
        self.run_git("-c", "user.name=Offline Test", "-c", "user.email=offline@example.invalid",
                     "commit", "-qm", "offline selected-source fixture")
        self.commit_sha = self.run_git("rev-parse", "HEAD").decode().strip()

    def test_complete_archive_preserves_blob_bytes_and_excludes_other_difficulties(self):
        # A changed checkout must not replace the pinned commit's raw bytes.
        self.put("charts/Exact.Artist.0/Chart_IN.json", b"modified checkout\r\n")
        metadata = self.root / "metadata.json"
        metadata.write_bytes(archive.json_bytes(dict(songs=[dict(song_id="Exact.Artist", title="Exact title",
                                       composer="Artist", ratings={"IN": 13})], source=dict(version="4.0.1.2"),
            charts=[dict(track_id="Exact.Artist.0", difficulty="IN", chart=dict(file="prior/IN.json",
                         sha256=hashlib.sha256(self.raw).hexdigest(), git_blob_sha=archive.blob_sha(self.raw), bytes=len(self.raw))),
                    dict(track_id="Exact.Artist.0", difficulty="AT", chart=dict(file="prior/AT.json",
                         sha256="0" * 64, git_blob_sha="0" * 40, bytes=1))])))
        manifest = archive.archive(self.source, self.commit_sha, self.output, metadata)
        self.assertEqual(manifest["counts"]["chart_count"], 2)
        self.assertEqual(manifest["counts"]["source_error_count"], 1)
        self.assertEqual(manifest["counts"]["unsupported_block_area_count"], 1)
        self.assertEqual(manifest["counts"]["unsupported_format_count"], 1)
        self.assertEqual(manifest["counts"]["catalog_byte_comparison"],
                         dict(exact_byte_identity=1, different_byte_identity=1))
        self.assertEqual((self.output / "charts/Exact.Artist.0/Chart_IN.json").read_bytes(), self.raw)
        self.assertFalse((self.output / "charts/Exact.Artist.0/Chart_EZ.json").exists())
        self.assertTrue((self.output / "source-errors/charts/Exact.Artist.0/Chart_IN_Error.json").is_file())
        library = json.loads((self.output / "library.json").read_text(encoding="utf-8"))
        self.assertEqual(library["schema"], "pas.catalog-library.v1")
        self.assertTrue(all(row["title"] == "Exact title" and row["live_ready"] is False and row["audio"] is None
                            for row in library["charts"]))
        self.assertEqual(archive.verify(self.output)["chart_count"], 2)

    def test_same_size_tamper_is_rejected_and_original_manifest_is_retained(self):
        archive.archive(self.source, self.commit_sha, self.output)
        manifest_raw = (self.output / "manifest.json").read_bytes()
        path = self.output / "charts/Exact.Artist.0/Chart_IN.json"
        path.write_bytes(self.raw[:-1] + b" ")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            archive.verify(self.output)
        self.assertEqual((self.output / "manifest.json").read_bytes(), manifest_raw)
        with self.assertRaisesRegex(ValueError, "immutable archive differs"):
            archive.archive(self.source, self.commit_sha, self.output)

    def test_lfs_pointer_is_not_published_as_a_complete_archive(self):
        self.put("charts/Pointer.Artist.0/Chart_IN.json", archive.LFS_HEADER + b"\noid sha256:" + b"0" * 64 + b"\nsize 123\n")
        self.commit()
        with self.assertRaisesRegex(ValueError, "LFS pointer"):
            archive.archive(self.source, self.commit_sha, self.output)
        self.assertFalse((self.output / "manifest.json").exists())

    def test_commit_mismatch_is_rejected_before_archive_creation(self):
        with self.assertRaisesRegex(ValueError, "checkout HEAD differs"):
            archive.archive(self.source, "0" * 40, self.output)
        self.assertFalse(self.output.exists())

    def test_manifest_checksum_and_unsafe_paths_are_rejected(self):
        archive.archive(self.source, self.commit_sha, self.output)
        with self.assertRaisesRegex(ValueError, "unsafe archive path"):
            archive.safe_path(self.output, "../outside.json")
        path = self.output / "manifest.json"
        path.write_bytes(path.read_bytes() + b" ")
        with self.assertRaisesRegex(ValueError, "manifest checksum mismatch"):
            archive.verify(self.output)


if __name__ == "__main__":
    unittest.main()
