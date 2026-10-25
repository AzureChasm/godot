# Weekly Godot updates

`update-godot.sh` fetches `upstream/master` and merges it into local `develop`.
It never pushes, switches branches, builds the engine, or updates local `master`.
Merge commits preserve custom module history without rewriting it. Divergent
history requires a configured Git author (`user.name` and `user.email`).

The script refuses dirty trees, another checked-out branch, and unfinished Git
operations. Conflicting merges are aborted; inspect the journal and integrate
the changes manually. Avoid editing the repository while an update runs.

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
