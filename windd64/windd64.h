/*
	windd64 (c) Luis González Fernández 2015
	luisgf@luisgf.es
	https://www.luisgf.es/
*/

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <iostream>
#include <queue>

#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#define MB_TO_BYTES(x) (x * 1024 * 1024)

typedef struct _bqueue {
	std::queue <LPVOID> *ptr;	// pointer to std::object	
	LONGLONG size;				// queue size, in bytes
} BQUEUE, *PBQUEUE;

typedef struct tParams {
	HANDLE hDev;
	HANDLE Mutex;
	DWORD SectorSize;
	DWORD StartSector;
	DWORD MemBuff;
	DWORD SectorsAtTime;
	LONGLONG DiskSectors;
	PBQUEUE cola;
} TPARAMS, *PTPARAMS;

/*
Data Structure. That holds de data readed from disk
*/
typedef struct _data {
	LPVOID ptr;						// pointer to data
	DWORD size;						// size of data stored
	LONGLONG sector_num;			// num of sectors readed/writed
} DATA, *PDATA;