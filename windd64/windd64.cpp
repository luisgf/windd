/*
	windd64 (c) Luis González Fernández 2015
	luisgf@luisgf.es
	https://www.luisgf.es/
*/

#include "windd64.h"

LPVOID xmalloc(size_t size) {
	LPVOID ptr = malloc(size);
	if (!ptr) {
		wprintf(L"xmalloc(%d): Memory allocation error\n", size);
		exit(-1);
	}
	return ptr;
}

VOID Usage(LPCTSTR ProgName) {
	wprintf(L"Usage: %s /if:INPUT /of:OUTPUT /buffer:200\n\n", ProgName);
	wprintf(L"Parameters:\n");
	wprintf(L"/?\t\t Show this help\n");
	wprintf(L"/if\t\t Input block device or file. Like \\\\.\\PhysicalDrive0\n");
	wprintf(L"/of\t\t Source block device or file. Like D:\\image.windd\n");
	wprintf(L"/buffer\t\t Amount of memory to use as buffer. In Mb, default=100\n");	
	wprintf(L"/ibs\t\t Input block size (in bytes)\n");
	wprintf(L"/obs\t\t Output block size (in bytes)\n");
	wprintf(L"/bs\t\t Block size (Input & Output), overwrite ibs and obs (in bytes)\n");
	wprintf(L"/skip\t\t Skip n bytes at input start\n");
	wprintf(L"/seek\t\t Seek n bytes at output\n");
	wprintf(L"/nd\t\t Hide the disclaimer banner.\n");
	wprintf(L"/v\t\t Verbose Output. Show more info about reader/writer parameters.\n");
	wprintf(L"\n\n");
	exit(0);
}

VOID Disclaimer() {	
	wprintf(L"WARNING: This program commes without warranty and can damage your computer\n");
	wprintf(L"if used incorrectly. Use with caution.\n\n");
}

DWORD WINAPI ReadSect(LPVOID lpParam)
{
	DWORD dwRead;								// bytes readed
	BOOL rs = FALSE;
	DWORD dwWaitResult;
	PDATA data = NULL;							// pointer to disk data readed
	PTPARAMS param = (PTPARAMS)lpParam;			// Thread parameters
	LONGLONG CurrentOffset = param->StartOffset;	
	DWORD BytesToRead = MB_TO_BYTES(20);		// read 20 mb at a time
	
	if (param->Verbose)
		wprintf(L"Reader(): StartOffset %lu, EndOffset: %lu, Size %lu, SectorSize: %lu\n", param->StartOffset, param->EndOffset, param->DiskSize, param->SectorSize);

	if (param->StartOffset) {
		LARGE_INTEGER liStart;
		liStart.QuadPart = param->StartOffset;
		SetFilePointerEx(param->hDev, liStart, NULL, FILE_BEGIN);
	}

	while (CurrentOffset != param->EndOffset) {		
		if (param->cola->size > param->MemBuff) {
			// Queue Full. Sleep...
			Sleep(100);
		}
		else {
			// estamos al final del dispositivo y no hay tantos bloques que leer.			
			if (param->EndOffset - CurrentOffset < BytesToRead)
				BytesToRead = (DWORD)(param->EndOffset - CurrentOffset);
			
			data = (PDATA)xmalloc(sizeof(DATA));
			data->size = BytesToRead;
			data->ptr = (LPVOID)xmalloc(data->size);
			rs = ReadFile(param->hDev, data->ptr, data->size, &dwRead, 0);  // read sector	

			if (dwRead != data->size) {
				wprintf(L"Error reading at offset: %lu\n", CurrentOffset);
				return -1;
			}

			dwWaitResult = WaitForSingleObject(param->Mutex, INFINITE); 

			switch (dwWaitResult)
			{
				// The thread got ownership of the mutex
				case WAIT_OBJECT_0:
					__try {
						param->cola->ptr->push(data);
						param->cola->size += BytesToRead;
						param->DataProcessed += BytesToRead;
					}
					__finally {
						ReleaseMutex(param->Mutex);
					}
				break;
			}
			CurrentOffset += BytesToRead;
			wprintf(L"\rCompleted: %.2f %%", (float)((float)(CurrentOffset * 100) / (float)param->DiskSize));
		}
	}
	if (param->Verbose)
		wprintf(L"\rData Readed: %lu bytes\n", param->DataProcessed);
	return rs;
}

DWORD WINAPI WriteSect(LPVOID lpParam)
{
	PTPARAMS param = (PTPARAMS)lpParam;	
	DWORD dwWaitResult;
	PDATA data = NULL;
	BOOL rs = FALSE;	
	ULONGLONG CurrentOffset = param->StartOffset;

	if (param->Verbose)
		wprintf(L"Writer(): StartOffset %lu, EndOffset: %lu, Size %lu, SectorSize: %lu\n", param->StartOffset, param->EndOffset, param->DiskSize, param->SectorSize);

	if (param->StartOffset) {
		LARGE_INTEGER liStart;
		liStart.QuadPart = param->StartOffset;
		SetFilePointerEx(param->hDev, liStart, NULL, FILE_BEGIN);
	}

	while (CurrentOffset != param->EndOffset) {
		dwWaitResult = WaitForSingleObject(param->Mutex, INFINITE);

		switch (dwWaitResult)
		{
			// The thread got ownership of the mutex
		case WAIT_OBJECT_0:
			__try {
				if (param->cola->size == 0) {
					// Queue depleted
					Sleep(100);
				}
				else {
					data = (PDATA)param->cola->ptr->front();

					DWORD written;					
					rs = WriteFile(param->hDev, data->ptr, data->size, &written, NULL);

					if (written > 0 && written < data->size) {
						wprintf(L"\nError, Out of space at destination.\n");
						exit(-1);
					}
					
					if (!rs) {
						wprintf(L"\nError Writing to destination. %lu\n", GetLastError());
						exit(-1);
					}					
					
					param->cola->ptr->pop();
					param->cola->size -= data->size;
					param->DataProcessed += data->size;										
					CurrentOffset += data->size;

					free(data->ptr);
					free(data);
					data = NULL;
				}
			}
			__finally {				
				ReleaseMutex(param->Mutex);
			}
			break;
		}
	}
	if (param->Verbose)
		wprintf(L"Data Writed: %lu bytes\n", param->DataProcessed);

	return rs;
}

/*
** Get the disk geometry info
*/

BOOL GetDescriptorGeometry(HANDLE hDevice, PDWORD SectorSize, PLONGLONG DiskSize) {
	BOOL bResult = FALSE;                 // results flag
	DWORD junk = 0;                     // discard results
	DISK_GEOMETRY_EX pdg = { 0 };
	STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR pAlignmentDescriptor = { 0 };	
	STORAGE_PROPERTY_QUERY Query;

	ZeroMemory(&Query, sizeof(STORAGE_PROPERTY_QUERY));
	Query.PropertyId = StorageAccessAlignmentProperty;

	/*
	* First  try using STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR, if not supported we revert
	* to DISK_GEOMETRY_EX
	*/

	bResult = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
		&Query, sizeof(STORAGE_PROPERTY_QUERY),
		&pAlignmentDescriptor, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
		&junk, NULL);
	
	if (bResult) {
		*SectorSize = pAlignmentDescriptor.BytesPerPhysicalSector;
	} 

	bResult = DeviceIoControl(hDevice,                       // device to be queried
			IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, // operation to perform
			NULL, 0,                       // no input buffer
			&pdg, sizeof(pdg),            // output buffer
			&junk,                         // # bytes returned
			(LPOVERLAPPED)NULL);          // synchronous I/O	

	if (bResult) {		
		if (!*SectorSize)
			*SectorSize = pdg.Geometry.BytesPerSector;

		*DiskSize = pdg.DiskSize.QuadPart;
	}
	else {
		// Ok, the descriptor must be a file rather than a block device
		LARGE_INTEGER FileSize;
		GetFileSizeEx(hDevice, &FileSize);
		*SectorSize = 4096;			// TODO: detect NTFS cluster Size
		*DiskSize = FileSize.QuadPart;
		bResult = TRUE;


		if (!FileSize.QuadPart) {
			wprintf(L"Unable to get descriptor Geometry\n");
			bResult = FALSE;
		}
	}

	return bResult;
}

/*
* Open the source and destination descriptors. Fail otherwhise.
*/
BOOL OpenDescriptors(LPTSTR InDev, LPTSTR OutDev, PHANDLE hInDev, PHANDLE hOutDev)
{
	DWORD desired_mask = (DWORD)(GENERIC_READ | GENERIC_WRITE);
	DWORD acces_mask = (DWORD)(FILE_SHARE_READ | FILE_SHARE_WRITE);

	*hInDev = CreateFileW(InDev, desired_mask, acces_mask, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	if (*hInDev == INVALID_HANDLE_VALUE)
	{
		wprintf(L"Open source descriptor error: %lu", GetLastError());
		CloseHandle(*hInDev);
		return FALSE;
	}
	
	SetFilePointer(*hInDev, 0, 0, FILE_BEGIN); // empezamos en el sector 0

	*hOutDev = CreateFile(OutDev, desired_mask, acces_mask, 0, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, 0);
	if (*hOutDev == INVALID_HANDLE_VALUE)
	{
		wprintf(L"Out destination descriptor error: %lu", GetLastError());
		CloseHandle(*hOutDev);
		return FALSE;
	}	
	SetFilePointer(*hOutDev, 0, 0, FILE_BEGIN); // empezamos en el sector 0

	return TRUE;
}

/*
	Parse de program arguments. Return TRUE all arguments are parsed correctly, FALSE otherwise.
	Valid parameters are:

	/if			Input device
	/of			Output device
	/bs			Block size for input and output (in bytes)
	    /ibs	Input block size (in bytes)
	    /obs	Output block size (in bytes)
	/buffer		Memory buffer (in Mb, default=100)
	/nd			No Disclaimer.

	Note: Block size are autodetected unless you force it with /ibs,/obs or /bs.
*/
BOOL ParseProgramArguments(PARGUMENTS pParams, DWORD args, _TCHAR **argv) {
	LPCTSTR param = NULL;	

	if (args < 2) {
		wprintf(L"Too few arguments. Use /? to get the help.\n");
		return FALSE;
	}

	// Default Values
	pParams->Verbose = FALSE;
	pParams->NoDisclaimer = FALSE;
	pParams->dwBuff = MB_TO_BYTES(100);
	pParams->dwSkip = 0;
	pParams->dwSeek = 0;

	// Skip argv[0], that is the path to the program executable.
	for (DWORD i = 1; i < args; i++){
		param = argv[i];
		if (param[0] != L'/' && param[0] != L'-') {
			wprintf(L"Wrong parameter: %s\n", param);
			return FALSE;
		}

		param++;	

		if (!_wcsnicmp(param, L"?", 1)) {
			Usage(argv[0]);
			exit(0);
		}
		if (!_wcsnicmp(param, L"if:", 3)) {
			pParams->sInDev = (LPTSTR)(param += 3);
		}
		if (!_wcsnicmp(param, L"of:", 3)) {
			pParams->sOutDev = (LPTSTR)(param += 3);
		}

		if (!_wcsnicmp(param, L"bs:", 3)) {
			LPCTSTR dwBs = (param += 3);
			pParams->dwOutBs = pParams->dwInBs = wcstol(dwBs, NULL, 10);
		} 
		if (!_wcsnicmp(param, L"ibs:", 4)) {
			LPCTSTR dwBs = (param += 4);
			// We set too, obs to the same value as ibs, unless /obs switch is used.
			pParams->dwOutBs = pParams->dwInBs = wcstol(dwBs, NULL, 10);
		}
		if (!_wcsnicmp(param, L"obs:", 4)) {
			LPCTSTR dwBs = (param += 4);
			pParams->dwOutBs = wcstol(dwBs, NULL, 10);
		}		
		if (!_wcsnicmp(param, L"buffer:", 7)) {
			LPCTSTR dwBuff = (param += 7);
			pParams->dwBuff = MB_TO_BYTES(wcstol(dwBuff, NULL, 10));
		}
		if (!_wcsnicmp(param, L"skip:", 5)) {
			LPCTSTR dwSkip = (param += 5);
			pParams->dwSkip = wcstol(dwSkip, NULL, 10);
		}
		if (!_wcsnicmp(param, L"seek:", 5)) {
			LPCTSTR dwSeek = (param += 5);
			pParams->dwSeek = wcstol(dwSeek, NULL, 10);
		}
		if (!_wcsnicmp(param, L"nd", 2)) {
			pParams->NoDisclaimer = TRUE;			
		}
		if (!_wcsnicmp(param, L"v", 1)) {
			pParams->Verbose = TRUE;
		}

		// Add next parameters parsing here...		
	}

	if (!pParams->sInDev) {
		wprintf(L"Input (/if) parameter missing.\n");
		return FALSE;
	}
	if (!pParams->sOutDev) {
		wprintf(L"Output (/of) parameter missing.\n");
		return FALSE;
	}
	if (!pParams->dwBuff) {
		pParams->dwBuff = MB_TO_BYTES(100);		
	}
	// Input block size checks
	if (pParams->dwInBs && (pParams->dwInBs < 512 || (pParams->dwInBs % 512) > 0)) {
		wprintf(L"Input block size must be multiple of 512.\n");
		return FALSE;
	}
	// Output block size checks
	if (pParams->dwOutBs && (pParams->dwOutBs < 512 || (pParams->dwOutBs % 512) > 0)) {
		wprintf(L"Output block size must be multiple of 512.\n");
		return FALSE;
	}

	return TRUE;
}


int _tmain(int argc, _TCHAR* argv[])
{
	ARGUMENTS params = { 0 };					// Parsed program arguments
	HANDLE hInDev = NULL;
	HANDLE hOutDev = NULL;

	// Disk Geometry
	LONGLONG DiskSize = { 0 };			// disk size in bytes	
	DWORD SectorSize;					// Physical sector size
	std::queue <LPVOID> cola;

	// Thread synchronization
	HANDLE hMutex;
	HANDLE hThread[2] = { 0 };
	DWORD ThreadID[2] = { 0 };

	if (!ParseProgramArguments(&params, argc, argv)) {
		return 1;
	}
	 
	BQUEUE data = { &cola, 0};					// data queue		 

#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken);
	DWORD infoLen;

	TOKEN_ELEVATION elevation;
	GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &infoLen);
	if (!elevation.TokenIsElevated)
	{
		wprintf(L"This program must run in elevated mode\n");
		return -1;
	}
#else
#error you are using an old version of sdk or not supported operating system
#endif	

	if (!OpenDescriptors(params.sInDev, params.sOutDev, &hInDev, &hOutDev))
	{
		return -1;
	}
	
	if (!GetDescriptorGeometry(hInDev, &SectorSize, &DiskSize))
	{
		return -1;	
	} 

	/* Mutex Creation */
	hMutex = CreateMutex(NULL, FALSE, NULL);

	if (hMutex == NULL)
	{
		wprintf(L"CreateMutex() error: %d\n", GetLastError());
		return -1;
	}

	/* The party start now	*/
	wprintf(L">>> windd %s - By Luis Gonzalez Fernandez\n", VERSION);
	if (!params.NoDisclaimer)
		Disclaimer();
	wprintf(L"%s => %s\n", params.sInDev, params.sOutDev);

	/* Reader Thread */
	TPARAMS ReaderParams = { 0 };
	ReaderParams.hDev = hInDev;
	ReaderParams.cola = &data;
	ReaderParams.StartOffset = params.dwSkip;			// skip n bytes at input
	ReaderParams.EndOffset = DiskSize;

	if (params.dwInBs)
		ReaderParams.SectorSize = params.dwInBs;
	else 
		ReaderParams.SectorSize = SectorSize;	

	ReaderParams.MemBuff = params.dwBuff;
	ReaderParams.Mutex = hMutex;	
	ReaderParams.DiskSize = DiskSize;
	ReaderParams.DataProcessed = 0;
	ReaderParams.Verbose = params.Verbose;

	hThread[0] = CreateThread(NULL, 0, ReadSect, &ReaderParams, 0, &ThreadID[0]);

	/* Writer Thread */
	TPARAMS WriterParams = { 0 };
	WriterParams.hDev = hOutDev;
	WriterParams.cola = &data;
	WriterParams.StartOffset = params.dwSeek;				// seek until this offset at write.
	WriterParams.EndOffset = (DiskSize + params.dwSeek - params.dwSkip);

	if (params.dwOutBs)
		WriterParams.SectorSize = params.dwOutBs;
	else
		WriterParams.SectorSize = SectorSize;
	
	WriterParams.Mutex = hMutex;	
	WriterParams.DiskSize = DiskSize;
	WriterParams.DataProcessed = 0;
	WriterParams.Verbose = params.Verbose;

	hThread[1] = CreateThread(NULL, 0, WriteSect, &WriterParams, 0, &ThreadID[1]);

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	if (ReaderParams.DataProcessed == WriterParams.DataProcessed)
		wprintf(L"Done!\n");
	else
		wprintf(L"Error, %lu bytes are not copied.\n", (ReaderParams.DataProcessed - WriterParams.DataProcessed));

	CloseHandle(hInDev);
	CloseHandle(hOutDev);

	return 0;
}
