"""Seal a new offline Windows runtime archive; never copy assets or run binaries."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


SOURCE_ROOTS = {"apps", "cmake", "include", "src", "tests", "tools", "proto", "third_party"}
SOURCE_FILES = {"CMakeLists.txt", "vcpkg.json", "vcpkg-configuration.json", "AGENTS.md"}
EXECUTABLES = {"chart_experiment.exe", "touch_probe.exe", "chart_inspect.exe", "grpc_patch_smoke.exe"}
MAX_FILES = 2048
MAX_BYTES = 1024 * 1024 * 1024


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def plain_file(path):
    # Resolve only after checking every component: junctions and symlinks may
    # otherwise silently include unrelated private files in a source snapshot.
    path = Path(path).absolute()
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part, "is_junction") and part.is_junction()):
            raise ValueError("Archive input contains a link: " + str(part))
    if not path.is_file():
        raise ValueError("Archive input is not a file: " + str(path))
    return path.resolve()


def collect(repo, binary_dir, evidence):
    files = []
    names = git(repo, "ls-files", "-z").decode("utf-8").split("\0")
    for name in names:
        relative = Path(name)
        if name and (name in SOURCE_FILES or relative.parts[0] in SOURCE_ROOTS):
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError("Tracked source path escapes repository")
            files.append((plain_file(repo / relative), Path("source") / relative))
    for name in sorted(EXECUTABLES):
        files.append((plain_file(binary_dir / name), Path("bin") / name))
    for path in sorted(binary_dir.iterdir()):
        if path.suffix.lower() == ".dll":
            files.append((plain_file(path), Path("bin") / path.name))
    for path in evidence:
        path = plain_file(path)
        files.append((path, Path("evidence") / path.name))
    destinations = [str(dst).casefold() for _, dst in files]
    if len(set(destinations)) != len(destinations):
        raise ValueError("Duplicate archive destination")
    if len(files) > MAX_FILES or sum(src.stat().st_size for src, _ in files) > MAX_BYTES:
        raise ValueError("Archive resource budget exceeded")
    return files


def archive(repo, binary_dir, output, evidence):
    repo, binary_dir, output = (Path(p).resolve() for p in (repo, binary_dir, output))
    if output.exists():
        raise FileExistsError("Archive output must be new")
    if not output.parent.is_dir():
        raise ValueError("Archive parent must exist")
    for root in (repo, binary_dir):
        if output.is_relative_to(root) or root.is_relative_to(output):
            raise ValueError("Archive output must be independent of source/build")
    if not evidence:
        raise ValueError("Explicit build/test evidence is required")
    head = git(repo, "rev-parse", "HEAD").decode().strip()
    status = git(repo, "status", "--porcelain=v1").decode("utf-8")
    files = collect(repo, binary_dir, evidence)
    expected = [(src, dst, src.stat().st_size, digest(src)) for src, dst in files]
    output.mkdir()
    artifacts = []
    # On failure retain partial copies for inspection; never emit a successful
    # manifest or delete an existing/partial archive.
    for src, relative, size, sha in expected:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, target)
        if target.stat().st_size != size or digest(target) != sha or digest(src) != sha:
            raise ValueError("Archive input changed during copy: " + str(src))
        artifacts.append(dict(file=relative.as_posix(), bytes=size, sha256=sha))
    for src, _, size, sha in expected:
        if src.stat().st_size != size or digest(src) != sha:
            raise ValueError("Archive input changed during sealing: " + str(src))
    if git(repo, "rev-parse", "HEAD").decode().strip() != head:
        raise ValueError("Repository HEAD changed during sealing")
    result = dict(schema="pas.adaptive-build-archive.v1", head_commit=head,
                  uncommitted_source_snapshot=bool(status), source_status=status,
                  binary_directory=str(binary_dir), artifacts=artifacts,
                  device_access=False, input_enabled=False, game_test="NOT RUN",
                  scope="New binary/source/evidence identities only; supplied logs retain their own validation scope. No gameplay qualification inherited.")
    (output / "manifest.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--binary-directory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, action="append", required=True)
    args = parser.parse_args()
    result = archive(args.repository, args.binary_directory, args.output, args.evidence)
    print(json.dumps(dict(artifacts=len(result["artifacts"]), head_commit=result["head_commit"], game_test=result["game_test"])))


if __name__ == "__main__":
    main()
