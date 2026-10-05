#!/usr/bin/env bash
# Supply the official pinned upstream single header; no network or system installation.
set -euo pipefail
[[ $# == 2 ]] || { echo 'usage: prepare_json_dependency.sh OFFICIAL_JSON_HPP FRESH_INCLUDE_ROOT' >&2; exit 64; }
upstream=$(realpath "$1")
dest=$(realpath -m "$2")
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
[[ ! -e "$dest" ]] || { echo 'refusing existing dependency directory' >&2; exit 64; }
printf '%s  %s\n' aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63 "$upstream" | sha256sum -c -
mkdir -p "$dest/single_include/nlohmann" "$dest/nlohmann"
cp "$upstream" "$dest/single_include/nlohmann/json.hpp"
patch --fuzz=0 -p1 -d "$dest" < "$here/fix-4736_char8_t.patch"
patch --fuzz=0 -p1 -d "$dest" < "$here/fix-4742_std_optional.patch"
cp "$dest/single_include/nlohmann/json.hpp" "$dest/nlohmann/json.hpp"
repo=$(git -C "$here" rev-parse --show-toplevel)
cp "$repo/docs/catalog/08-licenses/third_party/nlohmann-json.txt" "$dest/LICENSE.MIT"
printf '%s  %s\n' f2e1f3cac4e48fa851d6986291e4bd5b384d87f92c175d8b126cbd91580289cc "$dest/nlohmann/json.hpp" | sha256sum -c -
printf 'Prepared only an isolated include root: %s\n' "$dest"
