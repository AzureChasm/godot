#!/usr/bin/env bash
# Mirror upstream master locally and on origin; merge it into local develop.
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=${GODOT_REPO:-"$script_dir/../.."}
branch=${GODOT_BRANCH:-develop}
[[ "$branch" != master ]] || { printf 'GODOT_BRANCH must not be master.\n' >&2; exit 1; }
export GIT_TERMINAL_PROMPT=0
export GIT_MERGE_AUTOEDIT=no
export GIT_SSH_COMMAND=${GIT_SSH_COMMAND:-'ssh -o BatchMode=yes -o ConnectTimeout=20'}

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

log 'Fetching official upstream master.'
git -C "$repo" fetch --no-tags upstream '+refs/heads/master:refs/remotes/upstream/master'
before=$(git -C "$repo" rev-parse HEAD)
target=$(git -C "$repo" rev-parse refs/remotes/upstream/master)

# Recheck after the network operation in case editing started while fetching.
[[ $(git -C "$repo" symbolic-ref --quiet --short HEAD) == "$branch" ]] || fail 'Branch changed during fetch.'
[[ -z $(git -C "$repo" status --porcelain=v1 --untracked-files=all) ]] || fail 'Working tree changed during fetch.'

# Keep master an exact upstream mirror without checking it out or carrying
# maintenance files onto it. Refuse to move a branch used by another worktree.
worktrees=$(git -C "$repo" worktree list --porcelain)
if [[ $'\n'"$worktrees"$'\n' == *$'\nbranch refs/heads/master\n'* ]]; then
    fail 'master is checked out in another worktree; leave it detached or on another branch.'
fi
master_before=$(git -C "$repo" rev-parse --verify refs/heads/master)
git -C "$repo" merge-base --is-ancestor "$master_before" "$target" ||
    fail 'Local master has commits outside upstream master; refusing to overwrite them.'
git -C "$repo" update-ref -m 'Fast-forward clean master to upstream' refs/heads/master "$target" "$master_before"
log "Clean local master: $master_before -> $target"

# The explicit refspec publishes only the official commit, never develop or
# tags. A normal push refuses remote divergence; no force push is used.
git -C "$repo" -c push.followTags=false push origin "$target:refs/heads/master" ||
    fail 'Could not push origin/master. Local master is updated; develop is unchanged. Check authentication or remote divergence and rerun.'
log 'Published clean master to origin/master; develop remains local.'

if git -C "$repo" merge-base --is-ancestor "$target" HEAD; then
    log "$branch is already up to date."
    exit 0
fi

[[ $(git -C "$repo" symbolic-ref --quiet --short HEAD) == "$branch" ]] || fail 'Branch changed during push.'
[[ -z $(git -C "$repo" status --porcelain=v1 --untracked-files=all) ]] || fail 'Working tree changed during push.'

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
