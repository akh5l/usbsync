# USB Auto Backup Tool

A C++ program written for Linux that detects USB insertion and automatically syncs files (e.g. from `~/Documents`) to a USB drive. Designed for simple offline backups without cloud dependency.

## Features

- Detects inserted USB drives (Linux-only)
- Syncs files if they are new or updated
- Skips duplicates
- Lightweight and fast (C++17)

## How It Works

1. Identifies USB storage drives and allows the user to pick the desired one
2. Saves preferences in a configuration file
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
pacman -Syu && pacman -S g++ cmake
```

## Build & Run (Linux)

```bash
git clone https://github.com/akh5l/usbsync.git
cd usbsync
mkdir build && cd build
cmake ..
make
./usb-sync
