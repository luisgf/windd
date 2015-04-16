========================================================================
    windd64 (c) Luis González Fernández 2015
	luisgf@luisgf.es
========================================================================

windd64 v1.0.3:

This application implements a multithreaded, 64 bit version of a disk 
dumper for Windows Platform. 

This applications works cloning disks sectors readed from source in a
destination device or file. The final copy is a 1:1 of the source device.

This are the common dumps type that the application can make:

- Block Device over block device
- Block device over file
- File over block device

The memory footprint of this application is to low and can be tuned at start time
using the MemBuff parameter.


Usage: windd64.exe /if:INPUT /of:OUTPUT /buffer:200

Parameters:
/?               Show this help
/if              Input block device or file. Like \\.\PhysicalDrive0
/of              Source block device or file. Like D:\image.windd
/buffer          Amount of memory to use as buffer. In Mb, default=100
/ibs             Input block size (in bytes)
/obs             Output block size (in bytes)
/bs              Block size (Input & Output), overwrite ibs and obs (in bytes)
/skip            Skip n bytes at input start
/seek            Seek n bytes at output
/nd              Hide the disclaimer banner.
/v               Verbose Output. Show more info about reader/writer parameters.


-----------------------------------------------------------------------------------

Example 1) Clone a disk in a image file:
The following command will dump the Hard Disk 0 as image at location D:\image.raw 
using a 100 Mb of memory buffer.

C:\windd64>windd64.exe /if:\\.\PhysicalDrive0 /of:D:\image.raw /buffer:100

Example 2) Clone disk over other:
The following command will dump the Hard Disk 0 over Hard Disk 1 
using a 100 Mb of memory buffer.

C:\windd64>windd64.exe /if:\\.\PhysicalDrive0 /of:\\.\PhysicalDrive1 /buffer:100

Example 3) Restore a dump from file:
The following command will restore a image file over a disk using a 100 Mb
of memory buffer.

C:\windd64>windd64.exe /if:D:\image.raw /of:\\.\PhysicalDrive0 /buffer:100



