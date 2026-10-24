"""Re-run the C++ note export and compare every time to exact Python fractions."""
import argparse
from decimal import Decimal
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess


def rounded_ns(value):
    sign = -1 if value < 0 else 1
    quotient, remainder = divmod(abs(value.numerator), value.denominator)
    return sign * (quotient + int(2 * remainder >= value.denominator))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("chart", type=Path)
    parser.add_argument("--notes-output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.notes_output.exists() or args.report.exists():
        raise FileExistsError("Verifier outputs must be new files")
    if args.chart.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("Chart exceeds importer byte budget")
    original = args.chart.read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    chart = json.loads(original, parse_float=Decimal)
    result = subprocess.run([str(args.executable), str(args.chart), digest, "--notes-jsonl"],
                            capture_output=True, check=True, text=True, timeout=30)
    rows = [json.loads(line, parse_float=Decimal) for line in result.stdout.splitlines()]
    actual = {row["source_pointer"]: row for row in rows}
    if len(actual) != len(rows):
        raise ValueError("Duplicate C++ note source pointer")
    comparisons, start_error, end_error, expected_paths = 0, 0, 0, set()
    for line_index, line in enumerate(chart["judgeLineList"]):
        bpm = Fraction(line["bpm"])
        for side in ("notesAbove", "notesBelow"):
            for note_index, note in enumerate(line[side]):
                pointer = f"/judgeLineList/{line_index}/{side}/{note_index}"
                expected_paths.add(pointer)
                row = actual[pointer]
                if (row["source_id"] != digest + "#" + pointer or row["chart_sha256"] != digest
                        or row["line"] != line_index or row["source_index"] != note_index
                        or row["type"] != note["type"] or float(row["raw_tick"]) != float(note["time"])
                        or float(row["raw_hold_ticks"]) != float(note["holdTime"])
                        or float(row["bpm"]) != float(line["bpm"])
                        or row["side"] != ("above" if side == "notesAbove" else "below")):
                    raise ValueError("C++ note source or raw semantics mismatch: " + pointer)
                start = (Fraction(note["time"]) * Fraction(15, 8) / bpm + Fraction(chart["offset"])) * 10**9
                end = ((Fraction(note["time"]) + Fraction(note["holdTime"])) * Fraction(15, 8) / bpm
                       + Fraction(chart["offset"])) * 10**9
                start_error = max(start_error, abs(row["start_ns"] - rounded_ns(start)))
                end_error = max(end_error, abs(row["end_ns"] - rounded_ns(end)))
                comparisons += 1
    if expected_paths != set(actual) or start_error > 1 or end_error > 1:
        raise ValueError("C++ note time/pointer comparison failed")
    with args.notes_output.open("x", encoding="utf-8") as output:
        output.write(result.stdout)
    report = dict(chart_sha256=digest, cpp_executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
                  compared_notes=comparisons, compared_start_and_end_timestamps=comparisons * 2,
                  max_start_error_ns=start_error, max_end_error_ns=end_error,
                  notes_jsonl_sha256=hashlib.sha256(args.notes_output.read_bytes()).hexdigest(),
                  reference="Python exact Fraction arithmetic; nearest ns, half away from zero",
                  status="passed", scope="Every source pointer, type, side, raw tick, start and end; no game/device timing claim")
    with args.report.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2)
        output.write("\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
