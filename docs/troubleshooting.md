# Troubleshooting

Start with `mount-storage.log` next to the executable; the messages are
listed in [Configuration → the log](configuration.md#the-log).

## The disks are not mounted after a restart

| Check | How |
|-------|-----|
| The service is installed, automatic | `sc qc "Mount Storage"` — `START_TYPE: AUTO_START`, `BINARY_PATH_NAME` points to the current executable |
| It ran | `sc query "Mount Storage"` (normally `STOPPED` after its work); Event Viewer → System, source *Service Control Manager* |
| The dependency is set | `sc qc ProfSvc` — `DEPENDENCIES: RpcSs` and `Mount Storage` |
| The config | `mount-storage.cfg` next to the executable, full paths, see [Configuration](configuration.md) |
| By hand | administrator console, `mount-storage --run`, then read the log |
| Fast startup | after "Shut down" + power on, Windows resumes; the task `Mount Storage` handles it: `schtasks /Query /TN "Mount Storage" /V /FO LIST` |

Nothing in the log at all: either every disk was already attached
(normal on a fast startup resume), or the log could not be written (the
folder is not writable by SYSTEM).

## Windows error numbers

The `(error N)` in the log is a Windows error number. Look it up with
`net helpmsg N` or `certutil -error N`. The common ones:

| N | Meaning | Usual cause |
|---|---------|-------------|
| 2 | file not found | wrong path in the config, typo |
| 3 | path not found | folder missing, or the drive letter is not there (yet) |
| 5 | access denied | not run as administrator / SYSTEM; file permissions |
| 21 | device not ready | the drive is not ready, e.g. still locked by BitLocker |
| 32 | sharing violation | the file is in use, e.g. already attached under another path |
| 38 | reached end of file | (`unable to read`) the file is smaller than 64 KB — not a real disk |
| 1117 | I/O device error | drive problem |

For `unable to open` at mount time, an error from the Virtual Disk API
also means the file is not a valid VHD / VHDX of the expected type, for
example a `.vhd` renamed to `.vhdx`.

If a BitLocker data drive is unlocked late, enable *auto-unlock* for it
(`manage-bde -autounlock -enable D:`), so it unlocks with the system
drive.

## Logon is blocked

Because the User Profile Service depends on `Mount Storage`, removing or
breaking the service without `--uninstall` stops the User Profile Service
from starting: Windows reports that it *can not load the user profile*
or the *User Profile Service failed the sign-in*.

This happens if the service is deleted by hand, or its executable is
deleted, moved or blocked. (A missing config or a disk that can not be
mounted does **not** block logon — the service still starts.)

To recover, restore the default dependency, from any administrator
console (another account with a cached profile, Safe Mode with Command
Prompt, or the Windows Recovery Environment):

```
sc config ProfSvc depend= RpcSs
```

In the Recovery Environment the registry of the installed system is
offline; load it with `reg load` (or regedit → *Load Hive* on
`<windows drive>\Windows\System32\config\SYSTEM`) and set
`ControlSet001\Services\ProfSvc\DependOnService` to `RpcSs` only.

## Moving or removing the tool

- **Removing**: run `mount-storage --uninstall` first, then delete the
  folder.
- **Moving**: copy the files to the new folder, run
  `mount-storage --install` from there (it updates the service path, and
  replaces the task), then delete the old folder.
- **Updating** the executable: `mount-storage --stop` (normally already
  stopped), replace the file in place; no reinstall needed unless the
  folder changes. If a new release ships a changed
  `mount-storage.task.template.xml`, run `--install` again.

## Renaming the executable

The `.cfg` and `.log` names follow the executable name
(`storage.exe` → `storage.cfg`, `storage.log`). The task template and
the generated task file keep the names
`mount-storage.task.template.xml` / `mount-storage.task.xml`, and the
task runs `mount-storage.exe`, so keep the name `mount-storage.exe` if
the task is used.
