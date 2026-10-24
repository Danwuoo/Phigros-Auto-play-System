"""Synthetic source-contract checks; no copyrighted fixtures, device or UnityPy."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from extract_phigros_target import (chart_summary, extract, textasset_bytes,
                                    unique, verify_installed_source, write_new)


class ExtractionTests(unittest.TestCase):
    def test_exact_textasset_bytes_with_bom_and_newlines(self):
        payload = b'\xef\xbb\xbf{"test": "\\u4e00"}\r\n'
        for endian in ("<", ">"):
            name = b"Chart_IN"
            raw = struct.pack(endian + "i", len(name)) + name + struct.pack(endian + "i", len(payload)) + payload
            raw += b"\x00" * (-len(raw) % 4)
            parsed_name, script, offset = textasset_bytes(raw, endian)
            self.assertEqual(parsed_name, "Chart_IN")
            self.assertEqual(script, payload)
            self.assertEqual(offset, 16)

    def test_textasset_truncation_and_unknown_layout_rejected(self):
        for data in (b"", struct.pack("<i", -1) + b"xxxx", struct.pack("<i", 8000) + b"xxxx",
                     struct.pack("<ii", 0, -1), struct.pack("<ii", 0, 50),
                     struct.pack("<ii", 0, 0) + b"trailing"):
            with self.assertRaises(ValueError):
                textasset_bytes(data)

    def test_fingerprint_set_must_match_exactly(self):
        source = dict(package="com.PigeonGames.Phigros", version_name="4.0.1", version_code=157,
                      apk=[dict(file="base.apk", bytes=9, sha256="a" * 64),
                           dict(file="split.apk", bytes=10, sha256="b" * 64)])
        inventory = dict(packages=[dict(name=x["file"], bytes=x["bytes"], sha256=x["sha256"]) for x in source["apk"]])
        verify_installed_source(source, inventory)
        missing = copy.deepcopy(inventory)
        missing["packages"].pop()
        with self.assertRaises(ValueError): verify_installed_source(source, missing)
        changed = copy.deepcopy(inventory)
        changed["packages"][1]["sha256"] = "c" * 64
        with self.assertRaises(ValueError): verify_installed_source(source, changed)
        duplicate = copy.deepcopy(inventory)
        duplicate["packages"].append(duplicate["packages"][0])
        with self.assertRaises(ValueError): verify_installed_source(source, duplicate)

    def test_unique_resolution_rejects_ambiguity(self):
        self.assertEqual(unique([42], "fixture"), 42)
        for candidates in ([], [1, 2]):
            with self.assertRaises(ValueError): unique(candidates, "fixture")

    def test_no_overwrite_and_no_mismatched_track(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "chart.json"
            write_new(path, b"original")
            with self.assertRaises(FileExistsError): write_new(path, b"replacement")
            self.assertEqual(path.read_bytes(), b"original")
            with self.assertRaises(FileExistsError): extract([], "missing", temp, "x", "y")
            with self.assertRaises(ValueError):
                extract([], "missing", Path(temp) / "new", "Assets/Tracks/A/Chart_IN.json", "Assets/Tracks/B/music.wav")

    def test_raw_summary_counts_all_sides_and_preserves_offset(self):
        line = dict(bpm=120, notesAbove=[dict(type=1, time=32, holdTime=0), dict(type=3, time=64, holdTime=32)],
                    notesBelow=[dict(type=4, time=16, holdTime=0)], speedEvents=[], judgeLineMoveEvents=[],
                    judgeLineRotateEvents=[], judgeLineDisappearEvents=[])
        chart = dict(formatVersion=3, offset=-0.125, judgeLineList=[line])
        summary = chart_summary(json.dumps(chart).encode())
        self.assertEqual(summary["note_count"], 3)
        self.assertEqual(summary["sides"], dict(notesAbove=2, notesBelow=1))
        self.assertEqual(summary["offset_raw"], -0.125)
        self.assertEqual(summary["provisional_note_seconds_before_offset"], dict(first=0.25, last_start=1.0, last_end=1.5))
        chart["formatVersion"] = 4
        with self.assertRaises(ValueError): chart_summary(json.dumps(chart).encode())


if __name__ == "__main__":
    unittest.main()
