Fat32lib
========

Fat32lib is a portable FAT12/16/32 file system stack for embedded devices. All components, except for the Hardware Abstraction Layer (HAL) are written in portable ANSI C89 code to any platform with a C89 compiler. It is intended for embedded devices where memory and processing power are scarce resources.


Features:
---------

- Written in portable C (C89) (SD driver requires hardware abstraction library);
- Synchronous and asynchronous (non-blocking) I/O;
- Buffered and unbuffered I/O;
- Stream I/O (for writting continuos streams of data);
- Extensible drive manager (can be extended to interface with other file systems);
- FAT12/16/32 drive formatting support (can optimize file system for flash);
- SD card driver (for Microchip's dsPIC, but it should be easy to port);
- Driver supports any SD card size;
- Multi-threading support;
- Multiple volumes/partitions can be mounted at once;
- Automatic volume mounting (when SD card or other device is inserted);
- Long filenames support (ASCII only/can be disabled at compile-time);
- Several memory management options;
- Easy to use API;
- Released under GPL v3.

Feel free to browse the documentation <a href="http://fernando-rodriguez.github.io/fat32lib/doc/html/index.html" target="_blank">here</a>.

NOTE: Get the latest version here: http://sourceforge.net/projects/fat32lib/files/?source=directory
