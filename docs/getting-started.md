# Getting started

This walks through the set-up the tool was made for: a mirror (RAID1) of
two VHDX files, one on each physical drive, with Storage Spaces, mounted
automatically at start-up. For a single VHD/VHDX the steps are the same
without the Storage Spaces part.

All commands run in an **administrator** console (PowerShell for the
`*-VHD` and `Get-PhysicalDisk` commands; `Set-VHD` needs the Hyper-V
PowerShell module).

## 1. Get the tool

Either use a release archive (`xyo.mount-storage.<version>.win64-*.bin.zip`,
its `bin/` folder) or build it:

```
fabricare make        # output/bin/mount-storage.exe and mount-storage.task.template.xml
fabricare test        # optional, runs the tests
```

Both files from `bin/` are needed:

| File | Needed for |
|------|------------|
| `mount-storage.exe` | everything |
| `mount-storage.task.template.xml` | `--install` (creates the start-up task from it) |

## 2. Create the virtual disks

Create a `Storage` folder on each drive and one VHDX in each (for example
800 GB, dynamically expanding), with Disk Management or PowerShell:

```powershell
New-VHD -Path "C:\Storage\Storage-X-1.vhdx" -SizeBytes 800GB -Dynamic
New-VHD -Path "D:\Storage\Storage-X-2.vhdx" -SizeBytes 800GB -Dynamic
```

Storage Spaces requires **512 bytes physical sector size**. With the
disks unmounted:

```powershell
Set-VHD -Path "C:\Storage\Storage-X-1.vhdx" -PhysicalSectorSizeBytes 512
Set-VHD -Path "D:\Storage\Storage-X-2.vhdx" -PhysicalSectorSizeBytes 512
```

Mount them and check the sector size:

```powershell
Mount-VHD -Path "C:\Storage\Storage-X-1.vhdx"
Mount-VHD -Path "D:\Storage\Storage-X-2.vhdx"
Get-PhysicalDisk | sort-object SlotNumber | select SlotNumber, FriendlyName, Manufacturer, Model, PhysicalSectorSize, LogicalSectorSize | ft
```

## 3. Create the storage pool

Open **Control Panel → Storage Spaces**, create a new pool with the two
virtual disks and a **two-way mirror** space. Give it a drive letter
(for example `E:`).

## 4. Put the tool next to the disks

Copy `mount-storage.exe` and `mount-storage.task.template.xml` to a
folder that is on the system drive and stays there, for example
`C:\Storage`. The service runs the executable from this folder, so do not
move or delete it later without reading
[Troubleshooting → moving the tool](troubleshooting.md#moving-or-removing-the-tool).

Create `C:\Storage\mount-storage.cfg` (UTF-8, one **full path** per line;
see [Configuration](configuration.md)):

```
#
# List of vhd/vhdx to mount, full path
#
C:\Storage\Storage-X-1.vhdx
D:\Storage\Storage-X-2.vhdx
```

Without this file the tool does nothing (and logs
`Error: config file not found`).

## 5. Try it once

With the disks unmounted (eject them in Explorer or `Dismount-VHD`):

```
cd C:\Storage
mount-storage.exe --run
```

The disks are attached, the pool comes online, and
`C:\Storage\mount-storage.log` has a new line:

```
[ 2026-10-06 10:15:02 ] Mounted successfully
```

`--run` waits 30 seconds after a successful mount before it returns (as
the service does).

## 6. Install

```
cd C:\Storage
mount-storage.exe --install
```

This creates the **Mount Storage** service (automatic start, runs as
LocalSystem), makes the **User Profile Service** depend on it, and
creates the **Mount Storage** scheduled task for fast startup resume.
Details: [Command line → --install](command-line.md#--install).

Restart the computer. If everything is fine the Storage Spaces drive is
there after logon, and the log has a `Mounted successfully` line for the
boot. When the disks are still attached (fast startup resume) nothing is
logged.

## 7. Optional: move user data to the mirror

Now that the pool is mounted before any profile is loaded, user folders
(Documents, Desktop, ...) or whole profiles can be placed on it. Do this
only after a few restarts confirm the disks are mounted every time.

## Removing

```
cd C:\Storage
mount-storage.exe --uninstall
```

Removes the task, restores the User Profile Service dependency and
removes the service. The disks stay attached until the next restart;
the `.cfg`, `.log` and the generated `mount-storage.task.xml` are left in
the folder.
