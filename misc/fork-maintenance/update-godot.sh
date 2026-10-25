#!/usr/bin/env bash
# Merge upstream master into the local development branch. Never pushes.
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=${GODOT_REPO:-"$script_dir/../.."}
branch=${GODOT_BRANCH:-develop}
export GIT_TERMINAL_PROMPT=0
export GIT_MERGE_AUTOEDIT=no

log() { printf '[%s] %s\n' "$(date --iso-8601=seconds)" "$*"; }
fail() { log "ERROR: $*" >&2; exit 1; }

git -C "$repo" rev-parse --git-dir >/dev/null
lock=$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)/weekly-update.lock
exec 9>"$lock"
flock -n 9 || fail 'Another update is already running.'

[[ $(git -C "$repo" symbolic-ref --quiet --short HEAD) == "$branch" ]] ||
    fail "Check out $branch before updating; no branches were switched."
[[ -z $(git -C "$repo" status --porcelain=v1 --untracked-files=all) ]] ||
    fail 'Working tree has changes; commit or stash them before updating.'
for state in MERGE_HEAD CHERRY_PICK_HEAD REVERT_HEAD rebase-merge rebase-apply; do
    state_path=$(git -C "$repo" rev-parse --git-path "$state")
    [[ "$state_path" = /* ]] || state_path="$repo/$state_path"
    [[ ! -e "$state_path" ]] || fail "An unfinished Git operation exists: $state"
done

log "Fetching upstream master into local $branch (no push)."
git -C "$repo" fetch --no-tags upstream '+refs/heads/master:refs/remotes/upstream/master'
before=$(git -C "$repo" rev-parse HEAD)
target=$(git -C "$repo" rev-parse refs/remotes/upstream/master)

# Recheck after the network operation in case editing started while fetching.
[[ $(git -C "$repo" symbolic-ref --quiet --short HEAD) == "$branch" ]] || fail 'Branch changed during fetch.'
[[ -z $(git -C "$repo" status --porcelain=v1 --untracked-files=all) ]] || fail 'Working tree changed during fetch.'

if git -C "$repo" merge-base --is-ancestor "$target" HEAD; then
    log 'Already up to date.'
    exit 0
fi

cleanup_merge() {
    if git -C "$repo" rev-parse --verify --quiet MERGE_HEAD >/dev/null; then
        git -C "$repo" merge --abort || log 'ERROR: Automatic merge abort failed; inspect git status.'
    fi
}
trap cleanup_merge EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
if ! git -C "$repo" merge --no-edit "$target"; then
    fail 'Merge failed; aborting. Resolve the upstream changes manually and rerun.'
fi
log "Updated $branch: $before -> $(git -C "$repo" rev-parse HEAD)"
log 'Source updated locally. Rebuild the editor before using the new engine.'
