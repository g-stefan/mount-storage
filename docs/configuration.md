# Configuration and log

## Files next to the executable

The tool finds its files in the folder of the executable. The `.cfg` and
`.log` names follow the executable name (`mount-storage.exe` →
`mount-storage.cfg`, `mount-storage.log`); the task files have fixed
names.

| File | Written by | Purpose |
|------|------------|---------|
| `mount-storage.exe` | you | the tool, also the service and task executable |
| `mount-storage.cfg` | you | the list of disks to mount — **required** |
| `mount-storage.task.template.xml` | you (from `bin/`) | task definition template, read by `--install` |
| `mount-storage.task.xml` | `--install` | the task definition actually imported |
| `mount-storage.log` | the tool | one line per event, appended |

## `mount-storage.cfg`

A text file, one disk per line:

```
#
# List of vhd/vhdx to mount, full path
#
C:\Storage\Storage-X-1.vhdx
D:\Storage\Storage-X-2.vhdx
```

| Rule | Detail |
|------|--------|
| One **full path** per line | The service runs with `C:\Windows\System32` as current folder; a relative path would be looked up there. |
| `.vhd` or `.vhdx` | A path ending in `.vhd` (any case) is opened as VHD, anything else as VHDX. |
| Comments | A line whose first non-blank character is `#` is skipped. A `#` elsewhere is part of the path. |
| Blank lines | skipped |
| Whitespace | leading and trailing spaces / tabs are removed |
| Quotes | a path may be written in double quotes (`"C:\My Storage\A.vhdx"`), they are removed; not required for spaces |
| Encoding | UTF-8, with or without BOM (Notepad's default). Non ASCII paths work in UTF-8. Line ends CRLF or LF. |

An empty list (no disk lines) is logged as an error.

The paths are compared with the disks Windows already has attached
**ignoring case**, `/` vs `\` and a `\\?\` prefix, so a disk is not
attached twice.

## The log

`mount-storage.log` is appended to, never truncated. Each line starts with
the local date and time:

```
[ 2026-10-06 10:15:02 ] Mounted successfully
```

| Message | Meaning |
|---------|---------|
| `Mounted successfully` | every disk in the list is attached now, at least one by this run |
| *(nothing)* | every disk was already attached (normal after a fast startup resume) |
| `Error: process require elevation` | not run as administrator / SYSTEM |
| `Error: config file not found` | `mount-storage.cfg` missing or unreadable next to the executable |
| `Error: no vhd/vhdx in config file` | the config has only comments / blank lines |
| `Error: unable to open "<path>" (error N)` | the file can not be opened: missing, drive not available or locked (BitLocker), no access, or — at mount time — not a valid VHD/VHDX |
| `Error: unable to read "<path>" (error N)` | the file opens but its first 64 KB can not be read (too small, I/O error) |
| `Error: unable to mount "<path>" (error N)` | the virtual disk opened but Windows refused to attach it |

`N` is the Windows error number, see
[Troubleshooting](troubleshooting.md#windows-error-numbers). If the log
itself can not be opened (folder not writable) nothing is recorded.

When the service (or `--run`) finds a disk file not ready after 3 checks,
it logs the problem and **attaches none of the disks** — a mirror is
mounted completely or not at all at boot. `--task` instead attaches the
disks that are available and logs the others.
