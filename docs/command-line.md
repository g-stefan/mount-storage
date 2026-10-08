# Command line

```
mount-storage [--help | --usage | --license | --version]
mount-storage --install | --uninstall
mount-storage --start | --stop | --run
mount-storage --task          (used by the scheduled task)
mount-storage --service       (used by the Service Control Manager)
```

The executable's manifest requires administrator rights: started from a
normal console, Windows asks for elevation (UAC) and runs it in a new
console window. Use an administrator console to see the output.

## Rules

- Options start with two dashes (`--install`). Anything else is ignored.
- `--help`, `--usage`, `--license` and `--version` are printed in the
  order given, and can be combined with a command.
- **`--task`, `--install` and `--uninstall` are processed alone**: the
  first of them on the command line is the command, everything else is
  ignored.
- Otherwise the service commands `--start`, `--stop`, `--run` and
  `--service` are processed in order by the
  [xyo-win-service](https://github.com/g-stefan/xyo-win-service) base, so
  `--stop --start` restarts the service.
- No option at all, or no known option: the usage is printed, exit `1`.

## Options

| Option | What it does |
|--------|--------------|
| `--help`, `--usage` | print the usage (name, version, copyright, options) |
| `--license` | print the MIT license |
| `--version` | print `version X.Y.Z build N [date time]` |
| `--install` | install the service, the User Profile Service dependency and the start-up task |
| `--uninstall` | remove the task, restore the User Profile Service dependency, remove the service |
| `--start` | start the installed service and wait until it is running (the disks are mounted at that point) |
| `--stop` | stop the service and wait until it is stopped (the disks stay attached) |
| `--run` | do what the service does, in this console: mount, then wait 30 seconds |
| `--task` | like `--run`, but a single check of the disk files (no retry) and mounts the disks that are available even if some are not |
| `--service` | internal, the service entry point (`binPath` of the service) |

## `--install`

In order, stopping at the first failure:

1. **Service** — creates the service `Mount Storage`
   (`SERVICE_WIN32_OWN_PROCESS`, automatic start, LocalSystem), with the
   command `"<folder>\mount-storage.exe" --service`. If it already exists
   it is stopped and its command / start type are updated (so `--install`
   again after moving the executable fixes the path).
2. **User Profile Service dependency** — runs
   `sc config ProfSvc depend= "RpcSs/Mount Storage"`. Windows starts the
   User Profile Service only after `Mount Storage` reports running, which
   happens after the disks are attached.
3. **Task** — writes `mount-storage.task.xml` next to the executable from
   `mount-storage.task.template.xml` (the executable folder replaces
   `#{APPLICATION_PATH}`), then runs
   `schtasks /Create /XML mount-storage.task.xml /TN "Mount Storage" /F`
   (`/F` replaces a task left by a previous install).

Install does **not** start the service; restart Windows, or run
`--start`.

The task runs `mount-storage.exe --task` as SYSTEM, with highest
privileges, when the System event log records event **1** from
**Microsoft-Windows-Power-Troubleshooter** (the system resumed — this is
how a Windows 11 *fast startup* "boot" looks). See
[How it works](how-it-works.md#fast-startup-and-the-task).

## `--uninstall`

1. Deletes the task `Mount Storage` (no error if it does not exist).
2. Restores the User Profile Service dependency:
   `sc config ProfSvc depend= RpcSs` (the Windows default).
3. Stops and deletes the service (no error if it does not exist).

Attached disks stay attached until the next restart. The files next to
the executable are not removed.

## Exit codes

| Exit code | When |
|-----------|------|
| `0` | success; or only informational options (`--help`, `--license`, `--version`) |
| `1` | no option / no known option (usage printed); `--install` could not set the dependency, create the task file (template missing) or the task; `--uninstall` could not restore the dependency |
| Windows error number | `--install` / `--uninstall` / `--start` / `--stop` failed in the Service Control Manager, e.g. `5` access denied, `1060` service does not exist (for `--start` / `--stop`); the error is also printed: `Mount Storage: --install failed, error 5` |

`--run` and `--task` return `0`: problems mounting the disks are not
reported in the exit code, they are written to
[`mount-storage.log`](configuration.md#the-log).
