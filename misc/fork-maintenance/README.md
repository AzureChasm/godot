# Weekly Godot updates

`update-godot.sh` fetches official `upstream/master`, fast-forwards local `master`
to that exact commit, pushes only `master` to your fork (`origin`), then merges
that commit into local `develop`. It never pushes `develop` or tags, force-pushes,
switches branches, or builds the engine. Custom commits on local or remote
`master` cause the update to fail rather than overwrite them. Keep all custom
work, including these maintenance files, on `develop`.
Merge commits preserve custom module history without rewriting it. Divergent
history requires a configured Git author (`user.name` and `user.email`).

The script refuses dirty trees, another checked-out branch, and unfinished Git
operations. Conflicting merges are aborted; inspect the journal and integrate
the changes manually. Avoid editing the repository while an update runs.
`master` must not be checked out in another worktree. If publishing fails,
local `master` may already be updated, but `develop` is left unchanged; rerun
after fixing the problem. If merging into `develop` fails, the clean local and
remote `master` remain updated.

The timer needs noninteractive SSH access to `origin`. SSH uses batch mode and
fails instead of prompting for passwords or host-key confirmation. For an
encrypted key, ensure the user service can access an unlocked SSH agent
(including its `SSH_AUTH_SOCK` environment); an interactive terminal's agent is
not automatically available to systemd services. Check the service journal
after the first scheduled run.

Run manually from the Godot repository root:

```sh
bash misc/fork-maintenance/update-godot.sh
```

Install the user timer (unit paths match this workspace):

```sh
mkdir -p ~/.config/systemd/user
cp misc/fork-maintenance/systemd/godot-weekly-update.{service,timer} ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now godot-weekly-update.timer
systemctl --user list-timers godot-weekly-update.timer
```

The timer runs Fridays at 05:00 Europe/Berlin, including daylight-saving changes.
`Persistent=true` catches missed runs when the user service manager next starts.
It does not wake or power on the computer. To run while logged out, enable user
lingering with `loginctl enable-linger "$USER"` if permitted by the system.
An internet connection is required; failed attempts run again the following
Friday unless started manually.

```sh
journalctl --user -u godot-weekly-update.service -n 80 --no-pager
systemctl --user start godot-weekly-update.service  # update now
systemctl --user disable --now godot-weekly-update.timer  # stop scheduling
```

Rebuild the editor after updates. For a custom checkout or development branch,
the script accepts `GODOT_REPO` and `GODOT_BRANCH` environment variables.

Commit these maintenance files and `AGENTS.md` to the fork before the next
scheduled run: the updater intentionally refuses untracked files.
