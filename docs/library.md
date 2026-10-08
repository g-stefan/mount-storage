# Library and tests

`fabricare.json` builds the tool twice from the same sources:

| Project | Kind | Notes |
|---------|------|-------|
| `mount-storage.application` | static library | defines `XYO_MOUNTSTORAGE_LIBRARY` (no `main`), links `virtdisk`, `uuid`, `advapi32`, depends on `xyo-win-service` |
| `mount-storage` | executable | the tool; `fabricare/make.done.js` copies the task template to `output/bin` |
| `test.01`, `test.02` | tests (`category: test`) | depend on `mount-storage.application` |

Build and test:

```
fabricare make
fabricare test     # build + run the tests; run make first, test does not build the library
```

## The class

`XYO::MountStorage::Application` (`<XYO/MountStorage/Application.hpp>`)
derives from `XYO::WinService::Service`. Only one per process (the
service base keeps static state).

| Member | What it does |
|--------|--------------|
| `int main(int cmdN, char *cmdS[])` | the whole command line, see [Command line](command-line.md) |
| `int install()` / `int uninstall()` | `--install` / `--uninstall` |
| `DWORD serviceInit()` | runs `mountStorage()`, always `NO_ERROR` |
| `void serviceWork()` | waits 30 s after a successful mount |
| `bool mountStorage()` | the mount sequence; `true` if the disks were attached by this run |
| `static bool isTask` | `--task` mode: one check, partial mount allowed |

Static helpers, usable without the service (the tests use them):

| Helper | Returns |
|--------|---------|
| `initMemory()` | initializes the managed memory used by the helpers; call once from a plain `main` |
| `parseCommand(cmdN, cmdS)` | `Command::None / Task / Install / Uninstall / Service` |
| `parseConfig(content, vhdList)` | the disk list from the config file content |
| `getFilenameNearExecutable(executable, "cfg")` | the executable path with another extension |
| `isVHD(path)` | `true` for `.vhd` (any case) |
| `isSamePath(x, y)` | path equality ignoring case, `/` vs `\`, `\\?\` |
| `xmlEscape(text)` | `& < > " '` escaped |
| `createTaskFile(appPath, templateFile, taskFile)` | the UTF-16 task definition from the template |
| `checkVHDX(path, errorCode)` | `VHDXProblem::Ok / ErrorOpen / ErrorRead` (first 64 KB readable) |
| `mountVHDX(path, errorCode)` | `VHDXProblem::Ok / ErrorOpen / ErrorMount` — attaches for real, needs administrator |
| `getMountedVHDX(list)` | the disk files Windows has attached |
| `getDateTime()`, `isElevated()` | the log time stamp, the elevation check |

`VHDXProblem` (`None`, `ErrorOpen`, `ErrorRead`, `ErrorMount`,
`AlreadyMounted`, `Ok`) is the per disk state of the mount sequence.

Metadata: `XYO::MountStorage::Version::version()`, `build()`,
`datetime()`; `Copyright::copyright()`; `License::license()`.

## Tests

| Test | Covers |
|------|--------|
| `test/test.01.cpp` | `parseCommand` (precedence of `--task` / `--install` / `--uninstall`), `parseConfig` (comments, CRLF / LF, BOM, quotes, UTF-8, empty), `getFilenameNearExecutable` (only the file extension changes), `isVHD`, `isSamePath`, `xmlEscape` |
| `test/test.02.cpp` | `checkVHDX` on missing / empty / short / 64 KB / non ASCII named files and that the file is closed afterwards, `mountVHDX` errors for a missing and an invalid file, `getMountedVHDX` replaces the list, `createTaskFile` from the real template (UTF-16 BOM, path escaped) |

The tests do not need administrator rights and never attach a disk.
`test.02` runs in `output/test` and reads the template from
`../../source/XYO/MountStorage/`. Attaching a real disk is checked by
hand: `mount-storage --run` in an administrator console, then
`Get-VHD` / Disk Management, then detach.
