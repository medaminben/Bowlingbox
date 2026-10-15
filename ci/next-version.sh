#!/usr/bin/env bash
# Prints the version that follows the highest vX.Y.Z tag for a major, minor or patch bump.
# Without a tag, the fallback version in cmake/GitVersion.cmake is the current version.
set -euo pipefail

bump=${1:?usage: ci/next-version.sh major|minor|patch}
here=$(cd "$(dirname "$0")" && pwd)

current=$(git -C "$here" tag --list --sort=-v:refname |
    awk '/^v[0-9]+\.[0-9]+\.[0-9]+$/ && !found { print substr($0, 2); found = 1 }')
if [[ -z $current ]]; then
    current=$(sed -n 's/^set(BOWLINGBOX_FALLBACK_VERSION \([0-9.]*\))$/\1/p' "$here/../cmake/GitVersion.cmake")
fi
IFS=. read -r major minor patch <<<"$current"

case $bump in
    major) echo "$((major + 1)).0.0" ;;
    minor) echo "$major.$((minor + 1)).0" ;;
    patch) echo "$major.$minor.$((patch + 1))" ;;
    *) echo "unknown bump: $bump" >&2; exit 1 ;;
esac
