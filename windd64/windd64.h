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

#define VERSION L"v1.0.3"
#define MB_TO_BYTES(x) (x * 1024 * 1024)

typedef struct _bqueue {
	std::queue <LPVOID> *ptr;	// pointer to std::object	
	LONGLONG size;				// queue size, in bytes
} BQUEUE, *PBQUEUE;

/*
 Thread parameters structure
*/

typedef struct tParams {
	HANDLE hDev;				// device descriptor
	HANDLE Mutex;				// mutex to synchronize threads
	DWORD SectorSize;			// sector size	
	DWORD MemBuff;				// memory buffer in bytes
	LONGLONG StartOffset;		// start offset to read/write
	LONGLONG EndOffset;			// start offset to read/write
	LONGLONG DiskSize;			// disk size in bytes
	LONGLONG DataProcessed;		// amount of data processed by the thread
	PBQUEUE cola;				// pointer to queue of data
	BOOL Verbose;				// verbose
} TPARAMS, *PTPARAMS;

/*
Program Arguments structure
*/

typedef struct ProgArgumentss {
	LPTSTR sInDev;
	LPTSTR sOutDev;
	DWORD dwInBs;
	DWORD dwOutBs;
	DWORD dwBuff;
	DWORD dwSkip;
	DWORD dwSeek;
	BOOL NoDisclaimer;
	BOOL Verbose;
} ARGUMENTS, *PARGUMENTS;

/*
Data Structure. That holds de data readed from disk
*/
typedef struct _data {
	LPVOID ptr;						// pointer to data
	DWORD size;						// size of data stored	
} DATA, *PDATA;