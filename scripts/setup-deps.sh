#!/usr/bin/env sh
# Fetch DPF (and its nested pugl) and apply the patches this project needs.
#
# Kapibara builds against a PATCHED DPF: upstream has no uiClipboardData hook and
# pugl's X11 backend does not deliver file drops, both of which the UI relies on.
# Those patches used to exist only in one working tree, so a fresh clone could
# not build at all. They live in third_party/dpf-patches now and this script
# applies them; it is idempotent, so running it again on a patched tree is a
# no-op rather than an error.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

git submodule update --init --recursive

apply_patch() {
    repo=$1
    patch=$2
    if [ ! -s "$patch" ]; then
        return 0
    fi
    # --check first: already-applied patches must not be an error, or every
    # rebuild after the first would fail.
    if git -C "$repo" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "already applied: $patch"
        return 0
    fi
    echo "applying: $patch"
    git -C "$repo" apply "$patch"
}

apply_patch third_party/DPF                     "$root/third_party/dpf-patches/0001-uiClipboardData-and-file-drop.patch"
apply_patch third_party/DPF/dgl/src/pugl-upstream "$root/third_party/dpf-patches/0002-pugl-x11-file-drop.patch"

echo "dependencies ready"
