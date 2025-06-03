# FUSE Filesystem

A custom filesystem implementation using FUSE (Filesystem in Userspace). This project provides a bitmap-based filesystem implementation that can be mounted and used like any other filesystem on Unix-like operating systems.

## Features

- Bitmap-based storage management
- Efficient bit manipulation operations
- Pretty-printing of bitmap state
- FUSE integration for filesystem operations

## Prerequisites

- Unix-like operating system (Linux, macOS)
- FUSE library
- C compiler (GCC recommended)
- Make

## Installation

1. Clone the repository:
```bash
git clone https://github.com/Tejchak/FUSE-Filesystem.git
cd FUSE-Filesystem
```

2. Install dependencies:
```bash
# For Ubuntu/Debian
sudo apt-get install libfuse-dev

# For macOS
brew install macfuse
```

3. Compile the project:
```bash
make
```

## Usage

1. Mount the filesystem:
```bash
./fuse-filesystem /path/to/mount/point
```

2. Use the filesystem:
```bash
# Create files and directories
touch /path/to/mount/point/file.txt
mkdir /path/to/mount/point/directory

# List contents
ls -la /path/to/mount/point

# Unmount when done
fusermount -u /path/to/mount/point
```

## Project Structure

- `bitmap.h` - Header file containing bitmap interface declarations
- `bitmap.c` - Implementation of bitmap operations
  - `bitmap_get()` - Retrieve bit state
  - `bitmap_put()` - Set bit state
  - `bitmap_print()` - Visualize bitmap state
