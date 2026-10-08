# How it works

`mount-storage` is a Windows service built on
[xyo-win-service](https://github.com/g-stefan/xyo-win-service)
(`XYO::WinService::Service`). The whole tool is
`source/XYO/MountStorage/Application.cpp`.

## Start-up sequence

```
Service Control Manager starts "Mount Storage"   (automatic start)
  serviceMain
    report START_PENDING
    serviceInit()  ── mountStorage():
        open mount-storage.log (append)
        is the process elevated?                 no  -> log, stop
        read + parse mount-storage.cfg           no  -> log, stop
        list attached virtual disks              (GetAllAttachedVirtualDiskPhysicalPaths)
        mark listed disks already attached
        up to 3 times, 1 s apart:
            open each remaining file and read its first 64 KB
        any file not ready?                      yes -> log the problems, attach nothing
        attach each disk                         (OpenVirtualDisk + AttachVirtualDisk)
        log "Mounted successfully" or the errors
      return NO_ERROR   (always)
    report RUNNING  ──────────────────────────────>  User Profile Service may start now
    serviceWork(): if disks were attached, wait 30 s (or until stopped)
    report STOPPED
```

Points that matter:

- **The mount is done before the service reports running.** The User
  Profile Service depends on `Mount Storage`, so Windows starts it only
  after `Mount Storage` is *running* — at that point the disks are
  attached. While mounting, the service keeps the Service Control Manager
  informed (`reportPending`) so the start does not time out.
- **The service start never fails because of the disks.** `serviceInit`
  always returns success, even if the config is missing or a disk could
  not be attached; a failed start would also stop the User Profile Service
  from starting, and nobody could log on. Problems go to the log. COM is
  not initialized (not needed), so a COM problem can not fail the start
  either.
- **The service stops on its own** after the work (30 seconds after a
  successful mount, at once otherwise). The disks stay attached: they are
  attached with a *permanent lifetime*
  (`ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME`), not tied to the
  process.
- **Why read 64 KB first.** At boot the drive holding a disk file may not
  be available yet (BitLocker auto-unlock of a data drive happens during
  start-up). Reading the beginning of each file proves the volume is
  usable before anything is attached. Up to three checks one second apart
  give the volume time.
- **All or nothing at boot.** If a file is still not ready after three
  checks, nothing is attached: for a Storage Spaces mirror it is better to
  find the problem in the log than to bring the pool up degraded.

## The User Profile Service dependency

`--install` sets the dependency list of `ProfSvc` (User Profile Service)
to `RpcSs` (its Windows default) plus `Mount Storage`:

```
sc config ProfSvc depend= "RpcSs/Mount Storage"
```

`--uninstall` sets it back to `RpcSs`. This is stored in
`HKLM\SYSTEM\CurrentControlSet\Services\ProfSvc` →
`DependOnService`.

Consequence: **the `Mount Storage` service must exist and be startable**,
otherwise the User Profile Service does not start and no user can log on.
Always use `--uninstall` to remove the tool; see
[Troubleshooting](troubleshooting.md#logon-is-blocked) for recovery.

## Fast startup and the task

With *fast startup* (Windows 10 / 11 default), "Shut down" hibernates the
kernel session, and the next power-on resumes it instead of booting.
Services are not started again on resume. The disks were attached with a
permanent lifetime, so normally they are still attached after the resume
— but if a disk was detached in the meantime, nothing would attach it
again.

For that `--install` also creates the scheduled task **Mount Storage**
from `mount-storage.task.template.xml`:

| Task setting | Value |
|--------------|-------|
| Trigger | System log event ID 1 from `Microsoft-Windows-Power-Troubleshooter` (the system resumed from sleep / hibernation / fast startup) |
| Account | `S-1-5-18` (SYSTEM), highest privileges |
| Action | `<folder>\mount-storage.exe --task` |
| Multiple instances | ignore new |
| On battery | runs |

`--task` does the same as the service with a single check of the files
(the volumes are normally ready on resume), and attaches the disks that
are available even when some are not. When every disk is still attached
it does nothing and writes nothing to the log.

## The Virtual Disk API

For each disk:

```cpp
VIRTUAL_STORAGE_TYPE type = {VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,   // or _VHD for a .vhd file
                             VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};
OpenVirtualDisk(&type, path, VIRTUAL_DISK_ACCESS_ATTACH_RW, OPEN_VIRTUAL_DISK_FLAG_NONE, nullptr, &handle);
AttachVirtualDisk(handle, nullptr, ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, nullptr, nullptr);
CloseHandle(handle);
```

- The path from the config (UTF-8) is converted to UTF-16 and the wide
  API is used, so non ASCII paths work.
- No drive letter is assigned by the tool; Windows mounts the volumes of
  the disk as usual (for a Storage Spaces disk, the pool appears once all
  of its disks are attached).
- `GetAllAttachedVirtualDiskPhysicalPaths` lists the disk files Windows
  has attached; a listed disk found there is skipped.

## Running as SYSTEM, the files

The service runs as LocalSystem and the task as SYSTEM. Both find
`mount-storage.cfg` and `mount-storage.log` next to the executable (its
own path, not the current folder), so the folder must stay where it was
when `--install` ran.
