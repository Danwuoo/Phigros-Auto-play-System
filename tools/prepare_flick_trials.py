"""Prepare bounded one-parameter Flick trials; never run the game or send input.

The emitted PowerShell commands only invoke the native offline
``chart_experiment prepare-adaptive`` path. They create new profiles and plan
fingerprints; an existing profile is never patched under its old fingerprint.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path


DEFAULTS = dict(flick_lead_ms=30, flick_travel_ms=60,
                flick_distance_reference_pixels=60.0, flick_release_ms=24)
FLAGS = dict(flick_lead_ms="--flick-lead-ms", flick_travel_ms="--flick-travel-ms",
             flick_distance_reference_pixels="--flick-distance-pixels",
             flick_release_ms="--flick-release-ms")
PATH_POLICIES = ("dynamic-line-v1", "judgment-anchored-straight-v1")


def parameters(profile):
    result = {key: profile.get(key, value) for key, value in DEFAULTS.items()}
    for key, lower, upper in (("flick_lead_ms", 1, 100), ("flick_travel_ms", 2, 200),
                              ("flick_release_ms", 1, 100)):
        if type(result[key]) is not int or not lower <= result[key] <= upper:
            raise ValueError(f"{key} requires an integer in {lower}..{upper}")
    distance = result["flick_distance_reference_pixels"]
    if (type(distance) not in (int, float) or not math.isfinite(distance)
            or not 1 <= distance <= 240):
        raise ValueError("flick_distance_reference_pixels requires a finite number in 1..240")
    if result["flick_lead_ms"] >= result["flick_travel_ms"]:
        raise ValueError("Flick lead must be shorter than travel")
    # Normalize 60 and 60.0 so baseline/duplicate candidate detection is numeric.
    result["flick_distance_reference_pixels"] = float(distance)
    policy = profile.get("flick_path_policy", PATH_POLICIES[0])
    if type(policy) is not str or policy not in PATH_POLICIES:
        raise ValueError("Unknown Flick path policy")
    if policy != PATH_POLICIES[0]:
        result["flick_path_policy"] = policy
    return result


def read_json(path, limit):
    path = Path(path).resolve()
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError("Missing or oversized input: " + str(path))
    raw = path.read_bytes()
    result = json.loads(raw)
    if not isinstance(result, dict):
        raise ValueError("Input must be a JSON object")
    return result, hashlib.sha256(raw).hexdigest()


def reference(path, limit):
    path = Path(path).resolve()
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError("Missing or oversized input: " + str(path))
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return dict(file=str(path), sha256=digest.hexdigest())


def ps_literal(value):
    value = str(value)
    if any(char in value for char in "\r\n\x00"):
        raise ValueError("Command arguments must not contain line breaks/NUL")
    return "'" + value.replace("'", "''") + "'"


def prepare_trials(profile_path, chart_path, source_path, executable_path, output,
                   vary, values):
    if vary not in FLAGS or not 1 <= len(values) <= 12:
        raise ValueError("Choose one known parameter and 1..12 candidate values")
    output = Path(output).resolve()
    if output.exists():
        raise FileExistsError("Trial output must be new")
    profile, profile_sha = read_json(profile_path, 32768)
    source, source_sha = read_json(source_path, 1024 * 1024)
    if profile.get("schema") != "pas.adaptive-song-profile.v1":
        raise ValueError("An existing adaptive song profile is required")
    chart_ref = reference(chart_path, 32 * 1024 * 1024)
    executable_ref = reference(executable_path, 256 * 1024 * 1024)
    if (profile.get("chart_sha256") != chart_ref["sha256"]
            or source.get("chart", {}).get("sha256") != chart_ref["sha256"]
            or profile.get("source_manifest_sha256") != source_sha
            or source.get("track_id") != profile.get("song_id")
            or source.get("difficulty") != profile.get("difficulty")
            or source.get("decoded_audio", {}).get("sha256") != profile.get("audio_sha256")):
        raise ValueError("Baseline profile/chart/source identity mismatch")
    baseline = parameters(profile)
    drag_ns = profile.get("drag_duration_ns")
    if type(drag_ns) is not int or not 1_000_000 <= drag_ns <= 32_000_000 or drag_ns % 1_000_000:
        raise ValueError("Baseline Drag duration is not representable by native prepare CLI")
    tight_bounds = profile.get("tight_projection_speed_bounds", False)
    if type(tight_bounds) is not bool:
        raise ValueError("Baseline tight_projection_speed_bounds must be bool")
    common = ["--normal-projection-pixels", profile["max_normal_projection_reference_pixels"],
              "--drag-duration-ms", drag_ns // 1_000_000,
              "--input-inset-pixels", profile.get("input_inset_pixels", 0),
              "--contact-capacity", profile.get("contact_capacity", 4),
              "--hud-policy", profile.get("hud_policy", "calibrated-title-v1"),
              "--import-policy", profile.get("import_policy_id", "phigros-v3-strict-v1"),
              "--tight-projection-speed-bounds", int(tight_bounds)]
    if "flick_path_policy" in baseline:
        common.extend(("--flick-path-policy", baseline["flick_path_policy"]))
    variants = [("baseline", baseline)]
    seen = {json.dumps(baseline, sort_keys=True)}
    for value in values:
        candidate = parameters({**baseline, vary: value})
        key = json.dumps(candidate, sort_keys=True)
        if key in seen:
            continue
        seen.add(key)
        variants.append((f"candidate-{len(variants):02d}", candidate))
    if len(variants) == 1:
        raise ValueError("At least one candidate must differ from baseline")
    trials, commands = [], ["# Offline preparation only; no device discovery or touch input.",
                           "$ErrorActionPreference = 'Stop'"]
    for pinned in (dict(file=str(Path(profile_path).resolve()), sha256=profile_sha),
                   dict(file=str(Path(source_path).resolve()), sha256=source_sha), chart_ref, executable_ref):
        commands.append("if ((Get-FileHash -LiteralPath " + ps_literal(pinned["file"]) +
                        " -Algorithm SHA256).Hash.ToLowerInvariant() -ne " + ps_literal(pinned["sha256"]) +
                        ") { throw 'Pinned trial input changed; regenerate this preparation plan.' }")
    for label, candidate in variants:
        candidate_dir = output / label
        args = [executable_ref["file"], "prepare-adaptive", chart_ref["file"],
                str(Path(source_path).resolve()), str(candidate_dir), *common]
        for key, flag in FLAGS.items():
            args.extend((flag, candidate[key]))
        args = [str(value) for value in args]
        changed = [key for key in DEFAULTS if candidate[key] != baseline[key]]
        trials.append(dict(id=label, changed_parameters=changed, parameters=candidate,
                           output_directory=str(candidate_dir), native_prepare_argv=args,
                           preparation_status="not_run", gameplay_status="not_run"))
        commands.extend(("& " + " ".join(ps_literal(value) for value in args),
                         "if ($LASTEXITCODE -ne 0) { throw 'Native offline candidate preparation failed; inspect its report.json.' }"))
    result = dict(schema="pas.flick-trial-plan.v1", song_id=profile["song_id"],
                  difficulty=profile["difficulty"], varied_parameter=vary,
                  baseline_profile=dict(file=str(Path(profile_path).resolve()), sha256=profile_sha),
                  source_manifest=dict(file=str(Path(source_path).resolve()), sha256=source_sha),
                  chart=chart_ref, executable=executable_ref, trials=trials,
                  device_discovery=False, input_enabled=False, execution_performed=False,
                  winner=None, gameplay_qualified=False,
                  scope="One parameter per candidate; native preparation must regenerate and check fingerprints. No gameplay result is inferred.")
    output.mkdir(parents=True, exist_ok=False)
    (output / "trials.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (output / "prepare-commands.ps1").write_text("\n".join(commands) + "\n", encoding="utf-8-sig")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-profile", type=Path, required=True)
    parser.add_argument("--chart", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--vary", choices=tuple(FLAGS), required=True)
    parser.add_argument("--value", action="append", required=True,
                        help="Candidate JSON number; repeat for up to twelve values")
    args = parser.parse_args()
    try:
        values = [json.loads(value) for value in args.value]
        result = prepare_trials(args.baseline_profile, args.chart, args.source_manifest,
                                args.executable, args.output, args.vary, values)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, str(error) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
