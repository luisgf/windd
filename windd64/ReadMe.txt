========================================================================
    windd64 (c) Luis González Fernández 2015
	luisgf@luisgf.es
========================================================================

windd64:

This application implements a multithreaded, 64 bit version of a disk 
dumper for Windows Platform.

This software is capable of do the following dump types:

- Block Device over block device
- Block device over file
- File over block device

The memory footprint of this application can be tuned at start time using the
MemBuff parameter.


Usage: windd64.exe source destination BufferSize

Source:         A block device or file. Like \\.\PhysicalDrive0
Destination:    A block device or file. Like D:\image.windd
Buffer:         Amount of memory in MB to use as buffer. Like: 100

Example:
The following command will dump the Hard Disk 0 as image at location D:\image.raw 
using a 100 Mb of memory buffer.

C:\windd64>windd64.exe \\.\PhysicalDrive0 D:\image.raw 100




