---
name: mount-storage
description: >-
  How to use mount-storage, the XYO Windows service / command line tool
  (namespace XYO::MountStorage, on xyo-win-service) that attaches VHD /
  VHDX files automatically at Windows start-up, before the User Profile
  Service loads profiles — made for a Storage Spaces two-way mirror of two
  VHDX files on two physical (BitLocker) drives. Covers the set-up
  (New-VHD, Set-VHD -PhysicalSectorSizeBytes 512, the pool), the
  mount-storage.cfg list (full paths, # comments, UTF-8 / BOM, quotes),
  mount-storage.log messages and Windows error numbers, the options
  --install / --uninstall / --start / --stop / --run / --task / --service
  / --help / --license / --version and their exit codes, what --install
  changes (service "Mount Storage", sc config ProfSvc depend=
  RpcSs/Mount Storage, the scheduled task from
  mount-storage.task.template.xml on Power-Troubleshooter event 1 for fast
  startup), recovery when logon is blocked by the ProfSvc dependency, and
  the static library mount-storage.application (Application::mountStorage,
  parseConfig, checkVHDX, mountVHDX, getMountedVHDX, ...) and its tests.
  Use when setting up automatic VHD/VHDX mounting or a VHDX mirror on
  Windows, running or debugging mount-storage, reading its log, or working
  inside the mount-storage repository.
---

# mount-storage

Attaches **VHD / VHDX files automatically when Windows starts**, before
user profiles are loaded. It is a Windows service (`Mount Storage`) that
the **User Profile Service depends on**, plus a scheduled task for
Windows *fast startup* resume. Typical use: a RAID1-like Storage Spaces
mirror built from two VHDX files, one on each physical drive.

Windows only. The executable's manifest is `requireAdministrator`: run
it from an **administrator console** (from a normal one, UAC opens a new
window and the output is lost).

Full documentation: `docs/` in the repository
(`X:\Storage\XYO\Gitea\CPP\mount-storage\docs` on this machine):
README, getting-started, command-line, configuration, how-it-works,
troubleshooting, library. The whole tool is
`source/XYO/MountStorage/Application.cpp`.

## Set-up in short

```powershell
New-VHD -Path "C:\Storage\Storage-X-1.vhdx" -SizeBytes 800GB -Dynamic
New-VHD -Path "D:\Storage\Storage-X-2.vhdx" -SizeBytes 800GB -Dynamic
Set-VHD -Path "C:\Storage\Storage-X-1.vhdx" -PhysicalSectorSizeBytes 512   # Storage Spaces needs 512
Set-VHD -Path "D:\Storage\Storage-X-2.vhdx" -PhysicalSectorSizeBytes 512
# mount both, create the pool + two-way mirror in Control Panel -> Storage Spaces
```

Copy `mount-storage.exe` **and** `mount-storage.task.template.xml` (both
from `bin/`) to a permanent folder, e.g. `C:\Storage`, and create
`C:\Storage\mount-storage.cfg`:

```
# full paths, one per line
C:\Storage\Storage-X-1.vhdx
D:\Storage\Storage-X-2.vhdx
```

```
cd C:\Storage
mount-storage.exe --run        # try once (disks detached first), check mount-storage.log
mount-storage.exe --install    # service + ProfSvc dependency + task; then restart
```

## Hard rules

1. **Config**: `<exe name>.cfg` next to the executable. One **full path**
   per line (the service's current folder is System32). Lines starting
   with `#` and blank lines skipped, whitespace trimmed, optional double
   quotes, UTF-8 with or without BOM, CRLF or LF. `.vhd` (any case) opens
   as VHD, anything else as VHDX. Missing file → logged, nothing mounted.
2. **Everything is reported in `<exe name>.log`** next to the executable,
   appended, `[ YYYY-MM-DD hh:mm:ss ] ...`: `Mounted successfully`, or
   `Error: unable to open|read|mount "<path>" (error N)`,
   `Error: config file not found`, `Error: no vhd/vhdx in config file`,
   `Error: process require elevation`. Nothing logged = all disks were
   already attached. `--run` / `--task` exit `0` regardless.
3. **Boot is all or nothing**: each file must open and give its first
   64 KB (3 checks, 1 s apart, for late BitLocker unlock); if any is not
   ready, none is attached. `--task` does one check and attaches what is
   available.
4. **Mount happens before the service reports running**, and the start
   always succeeds (disk problems only go to the log), so ProfSvc starts
   after the disks are attached. Disks are attached with a permanent
   lifetime; the service stops itself (30 s after a successful mount).
5. **`--install`** = create/update service `Mount Storage` (auto start,
   LocalSystem, `"<exe>" --service`) → `sc config ProfSvc depend=
   "RpcSs/Mount Storage"` → write `mount-storage.task.xml` from the
   template and `schtasks /Create ... /TN "Mount Storage" /F` (SYSTEM,
   trigger System event 1 of Microsoft-Windows-Power-Troubleshooter =
   resume / fast startup, runs `--task`). It does not start the service.
   **`--uninstall`** = delete task → `sc config ProfSvc depend= RpcSs` →
   stop + delete service. Disks stay attached until restart.
6. **Never delete the service or the executable without `--uninstall`**:
   ProfSvc would depend on a missing service and nobody can log on.
   Recovery from any admin console / Safe Mode / WinRE:
   `sc config ProfSvc depend= RpcSs` (offline: `ControlSet001\Services\ProfSvc\DependOnService` = `RpcSs`).
   To move the tool: copy, `--install` from the new folder, delete the old.
7. **Options**: `--task`, `--install`, `--uninstall` are processed alone
   (first one wins). `--start`, `--stop`, `--run`, `--service` go to the
   xyo-win-service base, in order. `--help`/`--usage`/`--license`/`--version`
   print and can be combined. No known option → usage, exit `1`. Service
   failures exit with the Windows error (5 access denied, 1060 not
   installed) and print `Mount Storage: --<opt> failed, error N`.

Error numbers in the log: 2 file not found, 3 path not found (drive not
there yet), 5 access denied, 21 device not ready (BitLocker locked),
32 sharing violation, 38 file shorter than 64 KB; `net helpmsg N`.

## From C++

Depend on `mount-storage.application` (static lib, `XYO_MOUNTSTORAGE_LIBRARY`,
links virtdisk / uuid / advapi32), include `<XYO/MountStorage/Application.hpp>`.
`Application` derives from `XYO::WinService::Service` (one per process).
Static helpers, testable without administrator rights:

```cpp
using namespace XYO::MountStorage;
Application::initMemory();                       // once, from a plain main
TDynamicArray<String> vhdList;
Application::parseConfig(content, vhdList);      // config text -> paths
DWORD errorCode;
int problem = Application::checkVHDX(path, errorCode);   // VHDXProblem::Ok / ErrorOpen / ErrorRead
problem = Application::mountVHDX(path, errorCode);       // attaches for real (admin): Ok / ErrorOpen / ErrorMount
Application::getMountedVHDX(vhdList);            // attached disk files
Application::isSamePath(x, y);                   // case, / vs \, \\?\ insensitive
Application::parseCommand(cmdN, cmdS);           // Command::None / Task / Install / Uninstall / Service
Application::createTaskFile(appPath, templateFile, taskFile);
```

## Working inside this repository

- `fabricare.json`: `mount-storage.application` (lib), `mount-storage`
  (exe; `fabricare/make.done.js` copies the task template to
  `output/bin`), `test.01` / `test.02`. Version key `mount-storage`.
- Cycle: `fabricare make`, then `fabricare test` (it does not build the
  lib), `fabricare clean`. In the Claude Code shell clear
  `NoDefaultCurrentDirectoryInExePath` first (see the fabricare skill).
- The built exe can not be run from a non-elevated shell (manifest).
  Tests cover the logic through the library: `test.01` parsing / path
  helpers, `test.02` file checks, mount errors, task file (runs in
  `output/test`, reads `../../source/XYO/MountStorage/Application.Task.Template.xml`).
  They never attach a disk. Add cases there for any behavior change.
- The template is UTF-16 LE with BOM (binary in git); keep it that way.
- In `Application.cpp` use `XYO::System::Shell::` fully qualified
  (`Shell` alone is ambiguous with `XYO::Win::Shell`).
- Keep `docs/` and this skill in step with `Application.cpp`.
- **Licensing follows REUSE** (`python -m reuse lint`; `.reuse/dep5` is
  kept on purpose, its deprecation warning is expected). `source/`,
  `docs/`, `README.md`, `CHANGELOG.md` are MIT; `.clang-format`,
  `.gitignore`, `.vscode/*`, `.github/*` (CODE_OF_CONDUCT.md CC-BY-4.0),
  `.claude/*`, `fabricare.json`, `fabricare/*`, `version.json`, `test/*`
  are Unlicense. A new top level file or folder needs a `Files:` entry.
- **CRLF line endings**; new files come out LF, normalize them
  (`git ls-files --eol`). Do not use `sed -i` on existing files.
