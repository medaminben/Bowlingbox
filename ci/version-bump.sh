#!/usr/bin/env bash
# Prints the version bump (major, minor, patch or none) that a pull request's labels ask for.
# Labels: semver:major, semver:minor, semver:patch, semver:none. No semver label means patch.
# Fails if more than one semver label is given.
set -euo pipefail

bumps=()
for label in "$@"; do
    case $label in
        semver:major | semver:minor | semver:patch | semver:none) bumps+=("${label#semver:}") ;;
    esac
done

case ${#bumps[@]} in
    0) echo patch ;;
    1) echo "${bumps[0]}" ;;
    *) echo "more than one semver label: ${bumps[*]}" >&2; exit 1 ;;
esac
