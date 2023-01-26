# Mount Storage

Automatically mount VHD/VHDX on windows start-up

# Problem

I have a laptop, that has two disk slots. One is PCIe 4.0 NVMe, M.2 and the other SATA3 2.5 inch. Both are 1Tb size SSD.
The operating system is Windows 11, that sits on C: (NVMe drive), the other is D: (secondary drive).
Both disks are encrypted with Bitlocker for security purposes/requirements.
I want to have a RAID1 configuration, for data replication.
Windows bootloader does not support booting from Storage Spaces. And I don't want a dynamic drive configuration (mirror). I consider Storage Spaces more safe in case of failure.
I don't want Intel Raid or AMD Raid that uses BIOS/CPU combination. Some laptops/pc does not have this option.

# Solution

Create Storage folder on every disk.

Create two VHDX disks, 800Gb each. On every drive.
- C:\Storage\Storage-X-1.vhdx
- D:\Storage\Storage-X-2.vhdx

Unmount disks after creation.

Storage Spaces require 512 bytes sector size.

```powershell
Set-VHD -Path "C:\Storage\Storage-X-1.vhdx" -PhysicalSectorSizeBytes 512
Set-VHD -Path "D:\Storage\Storage-X-2.vhdx" -PhysicalSectorSizeBytes 512
```

Mount disks, check if sector size is ok.

```powershell
Get-PhysicalDisk | sort-object SlotNumber | select SlotNumber, FriendlyName, Manufacturer, Model, PhysicalSectorSize, LogicalSectorSize | ft
```

Go to Storage Spaces and create a new storage with those new virtual disks.

Copy ***mount-storage*** to C:\Storage

Create mount-storage.cfg file, it will not work otherwise.

```
#
# List of vhd/vhdx to mount, full path
#
C:\Storage\Storage-X-1.vhdx
D:\Storage\Storage-X-2.vhdx
```

Open an administrative console

```
cd C:\Storage
mount-storage.exe --install
```

The program will install as a service.
The User Profile Service will depend on the program.
The purpose is to be able to have user profile data stored on the new Storage Space disk.

Restart computer/laptop.

# Result

If everything is ok, you should have a Storage Spaces disk mounted.

The program will create a file, mount-storage.log, with date and time with operations performed.

```
[ 2023-01-27 00:18:35 ] Mounted successfully
```

## License

Copyright (c) 2023 Grigore Stefan
Licensed under the [MIT](LICENSE) license.
