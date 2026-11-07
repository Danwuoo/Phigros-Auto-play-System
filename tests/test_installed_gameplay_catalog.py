"""Offline tests preventing public/local byte aliases in the effective catalogue."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import derive_installed_gameplay_catalog as tool


class EffectiveCatalogTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.public_root = self.root / "public"
        self.public_root.mkdir()
        self.public = dict(schema="pas.catalog-library.v1", source=dict(version="4.0.1.2"),
            counts=dict(chart_count=4, song_count=4, live_ready_count=0), charts=[])
        self.variants, previous_rows = {}, []
        for index, (track, difficulty) in enumerate((*tool.TARGETS, ("Keep.Artist.0", "IN"))):
            relative = f"charts/{track}/Chart_{difficulty}.json"
            path = self.public_root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(f"public bytes {index}\r\n".encode())
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            old_id = "public/" + track + "/" + difficulty + "/" + digest
            self.public["charts"].append(dict(library_id=old_id, track_id=track, difficulty=difficulty,
                title=track, chart=dict(file=relative, sha256=digest, bytes=path.stat().st_size),
                summary=dict(block_area_fields=[]), analysis_status="structural_only", live_ready=False,
                prepare_status="blocked", source=dict(repository="public/source")))
            previous_rows.append(dict(library_id=old_id, track_id=track, difficulty=difficulty,
                chart_sha256=digest, status="rejected" if index < 3 else "verified_exact_installed_chart",
                reason="Public bytes differ" if index < 3 else None, source_manifest=None))
            if index < 3:
                actual = self.root / "installed" / relative
                actual.parent.mkdir(parents=True, exist_ok=True)
                actual.write_bytes(b"\xef\xbb\xbf" + f"installed bytes {index}\n".encode())
                actual_sha = hashlib.sha256(actual.read_bytes()).hexdigest()
                fields = [dict(path="/blockAreaList", state="empty", value_type="list", entries=0)] if index == 1 else []
                self.variants[(track, difficulty)] = dict(chart=dict(file=str(actual), sha256=actual_sha, bytes=actual.stat().st_size),
                    public_chart=dict(file=str(path), sha256=digest),
                    summary=dict(note_count=100+index, line_count=2, format_version=3, block_area_fields=fields),
                    analysis_status="unsupported_block_area" if fields else "structural_only",
                    blockers=[dict(code="unsupported_block_area", message="目前無法完成")] if fields else [],
                    source_manifest=dict(file=str(self.root / f"source-{index}.json"), sha256=str(index+1)*64),
                    audio_identity=dict(format="native-fsb5", sha256="a"*64))
        self.previous = dict(schema="pas.installed-catalog-identity.v1", charts=previous_rows,
                             total=4, verified=1, library=dict(file="original.json", sha256="1"*64))
        self.public_ref = dict(file=str(self.public_root / "library.json"), sha256="b"*64)
        self.installed_ref = dict(file=str(self.root / "installed-source.json"), sha256="c"*64)

    def tearDown(self):
        self.temporary.cleanup()

    def compose(self, public=None, variants=None):
        return tool.compose_catalog(self.public if public is None else public,
            self.variants if variants is None else variants, self.public_root, self.public_ref, self.installed_ref)

    def test_three_different_raw_sources_get_new_ids_and_original_inputs_stay_unchanged(self):
        before = copy.deepcopy(self.public)
        result = self.compose()
        self.assertEqual(self.public, before)
        self.assertEqual(result["charts"][-1]["library_id"], self.public["charts"][-1]["library_id"])
        self.assertEqual(result["charts"][-1]["chart"]["sha256"], self.public["charts"][-1]["chart"]["sha256"])
        for old, new in zip(self.public["charts"][:3], result["charts"][:3]):
            self.assertNotEqual(new["library_id"], old["library_id"])
            self.assertEqual(new["upstream_library_id"], old["library_id"])
            self.assertEqual(new["public_chart"]["sha256"], old["chart"]["sha256"])
            self.assertNotEqual(new["chart"]["sha256"], old["chart"]["sha256"])
            self.assertTrue(Path(new["chart"]["file"]).is_absolute())
            self.assertTrue(new["exact_local"])
            self.assertFalse(new["public_match"])
            self.assertFalse(new["live_ready"])

    def test_identity_proof_binds_actual_hashes_and_retains_public_provenance(self):
        before = copy.deepcopy(self.previous)
        target = dict(file="effective-catalog.json", sha256="d"*64)
        result = tool.compose_identity(self.previous, target, self.variants)
        self.assertEqual(self.previous, before)
        self.assertEqual(result["library"], target)
        self.assertEqual((result["total"], result["verified"]), (4, 4))
        for old, actual in zip(self.previous["charts"][:3], result["charts"][:3]):
            variant = self.variants[(actual["track_id"], actual["difficulty"])]
            self.assertEqual(actual["status"], "verified_exact_installed_chart")
            self.assertEqual(actual["chart_sha256"], variant["chart"]["sha256"])
            self.assertEqual(actual["source_manifest"], variant["source_manifest"])
            self.assertEqual(actual["upstream_library_id"], old["library_id"])
            self.assertNotIn("reason", actual)

    def test_actual_empty_block_area_stays_blocked_and_recomputes_counts(self):
        result = self.compose()
        blocked = result["charts"][1]
        self.assertEqual(blocked["analysis_status"], "unsupported_block_area")
        self.assertEqual(blocked["summary"]["block_area_fields"][0]["state"], "empty")
        self.assertIn("unsupported_block_area", [x["code"] for x in blocked["blockers"]])
        self.assertEqual(result["counts"]["unsupported_block_area_count"], 1)
        self.assertEqual(result["counts"]["block_area_field_states"], dict(empty=1))
        self.assertEqual(result["counts"]["live_ready_count"], 0)

    def test_identical_public_bytes_cannot_be_declared_a_different_variant(self):
        variants = copy.deepcopy(self.variants)
        variants[tool.TARGETS[0]]["chart"]["sha256"] = self.public["charts"][0]["chart"]["sha256"]
        with self.assertRaisesRegex(ValueError, "must not alias identical"):
            self.compose(variants=variants)

    def test_missing_or_duplicate_target_inventory_is_rejected(self):
        variants = copy.deepcopy(self.variants)
        variants.pop(tool.TARGETS[0])
        with self.assertRaisesRegex(ValueError, "Exactly the three"):
            self.compose(variants=variants)
        public = copy.deepcopy(self.public)
        public["charts"].append(copy.deepcopy(public["charts"][0]))
        with self.assertRaisesRegex(ValueError, "Duplicate track"):
            self.compose(public=public)

    def test_public_original_mismatch_or_escaping_reference_is_rejected(self):
        row = self.public["charts"][0]
        pin = tool.public_chart_pin(row, self.public_root)
        self.assertEqual(pin["sha256"], row["chart"]["sha256"])
        mutated = copy.deepcopy(row)
        mutated["chart"]["sha256"] = "0"*64
        with self.assertRaisesRegex(ValueError, "changed bytes"):
            tool.public_chart_pin(mutated, self.public_root)
        mutated["chart"]["file"] = "../outside.json"
        with self.assertRaisesRegex(ValueError, "escaped"):
            tool.public_chart_pin(mutated, self.public_root)

    def test_reject_existing_output_before_extraction_or_writing(self):
        marker = self.root / "retained-marker"
        marker.write_bytes(b"unchanged")
        with self.assertRaises(FileExistsError):
            tool.derive("unused-library", "unused-identity", "unused-source", [], self.root)
        self.assertEqual(marker.read_bytes(), b"unchanged")


if __name__ == "__main__":
    unittest.main()
