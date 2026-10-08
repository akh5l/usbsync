# USBSync (USB Auto Backup Tool)

A C++ program written for Linux that automatically syncs specified directories from `~/Documents` to a USB drive. Designed for simple offline backups without cloud dependency.

## Features

- Detects inserted USB drives (Linux-only)
- Allows selection between multiple USB drives and multiple partitions
- Syncs files if they are new or updated (skips duplicates)
- Lightweight and fast (C++17)
- Supports running as a systemd service with a timer

## How It Works

1. Identifies USB storage drives with `lsblk` and allows the user to pick the desired one
2. Saves preferences in a configuration file at `/etc/usbsync/usbsync.conf`
3. Mounts desired USB
4. Walks the source folder and copies new/updated files to USB target path

## Installing Dependencies

This program only requires a valid C++ compiler and CMake.

Debian-based distros:
```bash
sudo apt install g++ cmake
```

Arch Linux based distros:
```bash
sudo pacman -Syu g++ cmake
```

## Build

```bash
git clone https://github.com/akh5l/usbsync.git
cd usbsync
mkdir build && cd build
cmake ..
make
```
## Usage

This program must be ran as a superuser (with `sudo`) as it writes a config file to `/etc/` and writes files to `/mnt/`.  
Before setting it up as a systemd service, it must be ran manually with `sudo ./usbsync` once to set up the configuration file.

The systemd unit files should then be created as follows.  
Change the username field to the desired username and set timer intervals as necessary (default 5 minutes after boot, then every 10 minutes).

`/etc/systemd/system/usbsync.service`:

```bash
[Unit]
Description=USBSync Backup
After=local-fs.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/usbsync
Environment="USBSYNC_USER=YOUR_USERNAME_HERE"

[Install]
WantedBy=timers.target
```

`/etc/systemd/system/usbsync.timer`:

```bash
[Unit]
Description=Run USBSync Backup Periodically

[Timer]
OnBootSec=5min
OnUnitActiveSec=10min

[Install]
WantedBy=timers.target
```
