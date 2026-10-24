"""Offline synthetic chapter/source contracts; no native game fixtures or Unity."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import archive_phigros_chapter as chapter


class Catalog:
    def __init__(self, keys):
        self.keys = [SimpleNamespace(kind="string", value=key) for key in keys]
        self.entries = [SimpleNamespace(index=i, resource_type={"m_ClassName": "UnityEngine.AudioClip" if key.endswith("music.wav") else "UnityEngine.TextAsset"}) for i, key in enumerate(keys)]
        self.locations = {key: [entry] for key, entry in zip(keys, self.entries)}

    def lookup(self, key):
        return self.locations.get(key, [])


def fixture(root):
    raw = bytearray()
    identities = []

    def text(value):
        offset = len(raw)
        data = value.encode("utf-8")
        raw.extend(struct.pack("<i", len(data)) + data)
        raw.extend(b"\x00" * (-len(raw) % 4))
        return offset

    for value in ("SyntheticLegacy", "\\SyntheticTitle", "", "Synthetic"):
        identities.append(dict(length_offset=text(value), value=value))
    count_offset = len(raw)
    raw.extend(struct.pack("<i", 2))
    songs = []
    for order, track in enumerate(("光.測試.0", "Other.Composer.0"), 1):
        length_offset = text(track)
        raw.extend(struct.pack("<4i", -1, 0, 0, 0))
        difficulties = ["EZ", "HD", "IN"] if order == 1 else ["IN", "Legacy"]
        songs.append(dict(track_id=track, order=order, source_index=order - 1,
            source_metadata=dict(track_length_offset=length_offset, entry_end_offset=len(raw), native_trailing_i32_fields=[-1, 0, 0, 0]),
            charts=[dict(difficulty=d, catalog_key=f"Assets/Tracks/{track}/Chart_{d}.json") for d in difficulties],
            audio=dict(catalog_key=f"Assets/Tracks/{track}/music.wav")))
    record_end = len(raw)
    raw.extend(b"\x00" * 24)
    next_offset = text("SyntheticNext")
    blob = bytes(raw)
    (root / "object.bin").write_bytes(blob)
    proof = dict(schema=chapter.MEMBERSHIP_SCHEMA, package="com.PigeonGames.Phigros", version_name="4.0.1", version_code=157,
        chapter_id="SyntheticLegacy", chapter_name="Synthetic", song_count=2, songs=songs,
        proof_sources=[dict(file="object.bin", bytes=len(blob), sha256=chapter.sha256(blob))],
        catalog_sha256=chapter.sha256(b"catalog"), installed_source_sha256=chapter.sha256(b"installed"),
        native_provenance=dict(object_sha256=chapter.sha256(blob), object_bytes=len(blob),
            chapter_record_range_in_object=[0, record_end], chapter_record_sha256=chapter.sha256(blob[:record_end]),
            chapter_identity_fields=identities, native_song_count_offset=count_offset,
            next_chapter_identity=dict(length_offset=next_offset, value="SyntheticNext")))
    path = root / "membership.json"
    path.write_bytes(chapter.json_bytes(proof))
    return proof, path


class Session:
    def __init__(self, proof):
        self.catalog = Catalog([c["catalog_key"] for s in proof["songs"] for c in s["charts"]] + [s["audio"]["catalog_key"] for s in proof["songs"]])
        self.catalog_bytes = b"catalog"
        self.catalog_origin = dict(sha256=chapter.sha256(self.catalog_bytes))
        self.installed_source_sha256 = chapter.sha256(b"installed")
        self.source = dict(package="com.PigeonGames.Phigros", version_name="4.0.1", version_code=157)
        self.inventory = dict(packages=[], total_entries=8)
        self.max_cached_bundle_bytes = 512 * chapter.MIB
        self.bundle_cache = {}
        self.post_checks = 0

    def verify_sources_unchanged(self):
        self.post_checks += 1


def fake_audio(session, key, output, *, writer):
    native = writer(output / "music.fsb", b"FSB5-synthetic")
    writer(output / "music.audioclip.bin", b"synthetic-object")
    decoded = writer(output / "music.wav", b"synthetic-wav")
    return native, decoded


def fake_chart(session, key, output, *, difficulty, raw_name, writer, allow_unsupported_summary):
    payload = (key + "\r\n").encode("utf-8")
    artifact = writer(output / f"Chart_{difficulty}.json", payload)
    writer(output / raw_name, b"synthetic-raw-object")
    return artifact, dict(note_count=1, format_version=3)


class ChapterArchiveTests(unittest.TestCase):
    def test_exact_unicode_windows_leaves_and_rejections(self):
        self.assertEqual(chapter.safe_leaf("光.姜米條.0"), "光.姜米條.0")
        for value in ("", ".", "..", "a/b", "a\\b", "C:secret", "CON", "com1.txt", "LPT¹", "tail.", "tail ", "bad\x00name"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                chapter.safe_leaf(value)

    def test_native_record_proves_count_order_and_exact_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            proof, path = fixture(root)
            actual, _, files = chapter.load_membership(path)
            self.assertEqual([s["track_id"] for s in actual["songs"]], [s["track_id"] for s in proof["songs"]])
            for mutation in ("count", "track", "order", "trailing", "boundary"):
                changed = copy.deepcopy(proof)
                if mutation == "count": changed["song_count"] = 1
                if mutation == "track": changed["songs"][0]["track_id"] = "Guessed.Name.0"
                if mutation == "order": changed["songs"].reverse()
                if mutation == "trailing": changed["songs"][0]["source_metadata"]["native_trailing_i32_fields"][0] = 3
                if mutation == "boundary": changed["native_provenance"]["next_chapter_identity"]["value"] = "wrong"
                with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                    chapter.validate_native_record(changed, files)

    def test_proof_files_cannot_escape_or_change(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            proof, path = fixture(root)
            proof["proof_sources"][0]["file"] = "../outside.bin"
            path.write_bytes(chapter.json_bytes(proof))
            with self.assertRaises(ValueError): chapter.load_membership(path)
            proof, path = fixture(root)
            (root / "object.bin").write_bytes(b"changed")
            with self.assertRaises(ValueError): chapter.load_membership(path)

    def test_catalog_all_difficulties_and_variant_are_not_guessed(self):
        track = "光.測試.0"
        keys = [f"Assets/Tracks/{track}/Chart_{d}.json" for d in ("IN", "Legacy", "EZ", "HD", "AT")]
        keys += ["Assets/Tracks/Other/Chart_IN.json", f"Assets/Tracks/{track}/illustration.png"]
        charts = chapter.catalog_charts(Catalog(keys), track)
        self.assertEqual([c["difficulty"] for c in charts], ["EZ", "HD", "IN", "AT", "Legacy"])
        self.assertEqual(charts[-1]["classification"], "asset_variant")
        self.assertTrue(all(c["ui_selectability"] == "unverified" for c in charts))

    def test_ambiguous_missing_and_case_colliding_charts_fail(self):
        key = "Assets/Tracks/A/Chart_IN.json"
        catalog = Catalog([key])
        catalog.locations[key].append(catalog.locations[key][0])
        with self.assertRaises(ValueError): chapter.catalog_charts(catalog, "A")
        with self.assertRaises(ValueError): chapter.catalog_charts(Catalog([]), "A")
        with self.assertRaises(ValueError): chapter.catalog_charts(Catalog([key, "Assets/Tracks/A/Chart_in.json"]), "A")

    def test_chart_helper_refuses_a_mislabeled_catalog_difficulty(self):
        with self.assertRaises(ValueError):
            chapter.extract_chart_asset(None, "Assets/Tracks/A/Chart_HD.json", "unused", difficulty="IN")

    def test_writer_refuses_path_escape_overwrite_and_total_budget(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "archive"
            writer = chapter.ArchiveWriter(root, max_bytes=4)
            writer.write(root / "x", b"abc")
            with self.assertRaises(FileExistsError): writer.write(root / "x", b"d")
            with self.assertRaises(ValueError): writer.write(root / "y", b"de")
            with self.assertRaises(ValueError): writer.write(root.parent / "outside", b"d")

    def run_archive(self, root, proof, path, *, chart_function=fake_chart, audio_function=fake_audio):
        session = Session(proof)
        output = root / "archive"
        with patch.object(chapter, "verify_native_source") as native_check, patch.object(chapter.importlib.metadata, "version", return_value="synthetic"), \
             patch.object(chapter, "extract_chart_asset", side_effect=chart_function) as chart_call, \
             patch.object(chapter, "extract_audio_asset", side_effect=audio_function) as audio_call:
            index = chapter.archive_chapter([], root / "installed.json", path, output, session_factory=lambda *_: session)
        return output, index, session, native_check, chart_call, audio_call

    def test_batch_archives_every_chart_and_only_one_audio_per_song(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            proof, path = fixture(root)
            output, index, session, native, charts, audio = self.run_archive(root, proof, path)
            self.assertEqual(index["status"], "complete")
            self.assertEqual(index["chart_count"], 5)
            self.assertEqual(audio.call_count, 2)
            self.assertEqual(charts.call_count, 5)
            self.assertEqual(native.call_count, 1)
            self.assertEqual(session.post_checks, 1)
            self.assertEqual(len(list(output.rglob("music.wav"))), 2)
            for song in index["songs"]:
                for row in song["charts"]:
                    chart = output / row["chart"]["file"]
                    self.assertEqual(chapter.hash_file(chart), row["chart"]["sha256"])
                    source = output / row["source_manifest"]["file"]
                    self.assertEqual(json.loads(source.read_text())["chart"]["sha256"], row["chart"]["sha256"])
                    self.assertTrue((source.parent / "music.wav").is_file())
            manifest = json.loads((output / "archive-manifest.json").read_text())
            for item in manifest["artifacts"]:
                self.assertEqual(chapter.hash_file(output / item["file"]), item["sha256"])

    def test_unknown_chart_structure_is_archived_but_incomplete(self):
        def unsupported(*args, **kwargs):
            artifact, summary = fake_chart(*args, **kwargs)
            if kwargs["difficulty"] == "Legacy":
                summary = dict(status="unsupported_structural_summary", reason="synthetic unknown dialect")
            return artifact, summary
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            proof, path = fixture(root)
            output, index, *_ = self.run_archive(root, proof, path, chart_function=unsupported)
            self.assertEqual(index["status"], "incomplete")
            self.assertEqual(index["chart_count"], 5)
            self.assertEqual(index["failures"][0]["code"], "unsupported_chart_structure")
            self.assertTrue(next(output.rglob("Chart_Legacy.json")).is_file())

    def test_existing_output_rejected_before_any_source_session(self):
        with tempfile.TemporaryDirectory() as temp:
            with patch.object(chapter, "load_membership") as load:
                with self.assertRaises(FileExistsError): chapter.archive_chapter([], "missing", "missing", temp)
                load.assert_not_called()

    def test_partial_failure_keeps_prior_artifacts_and_explicit_failed_manifest(self):
        calls = 0
        def broken(*args, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2: raise ValueError("synthetic decode failure")
            return fake_audio(*args, **kwargs)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            proof, path = fixture(root)
            with self.assertRaises(ValueError): self.run_archive(root, proof, path, audio_function=broken)
            output = root / "archive"
            failure = json.loads((output / "failure-manifest.json").read_text())
            self.assertEqual(failure["status"], "failed")
            self.assertFalse(failure["complete"])
            self.assertEqual(failure["completed_songs"], [proof["songs"][0]["track_id"]])
            self.assertFalse((output / "chapter-index.json").exists())
            self.assertTrue((output / "songs" / proof["songs"][0]["track_id"] / "Chart_IN.json").is_file())


if __name__ == "__main__":
    unittest.main()
